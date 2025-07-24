#![no_std]
/* use the C allocator; don't use libstd */
extern crate alloc;
use alloc::boxed::Box;
use libc_alloc::LibcAlloc;

#[global_allocator]
static ALLOCATOR: LibcAlloc = LibcAlloc;

/* we only support panic = abort; define dummy handler and EH personality */
#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}

#[no_mangle]
extern "C" fn rust_eh_personality() {}

#[link(name = "gcc_s")]
extern "C" {}

/* print errors via libc */
macro_rules! printerrln {
    ($($items: expr),+) => {{
        let s = alloc::format!($($items,)+);
        extern "C" { fn write(fd: i32, buf: *const u8, len: usize); }
        unsafe {write(2, s.as_ptr(), s.len()); write(2, "\n".as_ptr(), 1);}
    }}
}

#[repr(C)]
#[derive(Copy, Clone, PartialEq, Eq)]
pub struct Range {
    pub start: usize,
    pub len: usize,
}

use core::fmt;

impl fmt::Debug for Range {
    fn fmt(&self, fmt: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(fmt, "[{:08x}, +{:08x})", self.start, self.len)
    }
}

impl Range {
    pub fn from_bounds(start: usize, end: usize) -> Option<Range> {
        if start < end {
            Some(Range {
                start,
                len: end - start,
            })
        } else {
            None
        }
    }
    pub fn end(&self) -> usize {
        self.start + self.len
    }
    pub fn as_std(&self) -> core::ops::Range<usize> {
        self.start..self.end()
    }
    pub fn subsumes(&self, subrange: Range) -> bool {
        self.start <= subrange.start && self.end() >= subrange.end()
    }
    fn round_to_4k(&mut self) {
        let end = self.start + self.len;
        let end_round_up = (end + 0xFFF) & !0xFFF;
        self.start = self.start & !0xFFF;
        self.len = end_round_up - self.start;
    }

    pub fn overlap(&self, other: &Range) -> Option<Range> {
        let start = self.start.max(other.start);
        let end = self.end().min(other.end());
        Self::from_bounds(start, end)
    }

    /// Remove the other range from this one.
    /// If they do not overlap, returns None.
    /// Otherwise, returns a pair of any prefix remaining from this range and any suffix remaining from this range.
    pub fn subtract(&self, other: &Range) -> Option<(Option<Range>, Option<Range>)> {
        match self.overlap(other) {
            None => None,
            Some(overlap) => Some((
                Self::from_bounds(self.start, overlap.start),
                Self::from_bounds(overlap.end(), self.end()),
            )),
        }
    }
}

/// The state of a contiguous region of memory
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct State {
    pub owner_pkey: u8,
    pub pkey_mprotected: bool,
    pub mprotected: bool,
    pub prot: u32,
}

/// A contiguous region of the memory map whose state we track
#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct MemRegion {
    pub range: Range,
    pub state: State,
}

use nonoverlapping_interval_tree as disjoint_interval_tree;

use disjoint_interval_tree::NonOverlappingIntervalTree;

#[derive(Clone)]
pub struct MemoryMap {
    regions: NonOverlappingIntervalTree<usize, State>,
    init_finished: bool,
}

impl MemoryMap {
    pub fn new() -> Self {
        MemoryMap {
            regions: Default::default(),
            init_finished: false,
        }
    }
    pub fn mark_init_finished(&mut self) -> bool {
        if self.init_finished {
            false
        } else {
            self.init_finished = true;
            true
        }
    }
    pub fn add_region(&mut self, mut range: Range, state: State) -> bool {
        // forbid zero-length regions
        if range.len == 0 {
            return false;
        }
        // round size up and start down to pages
        range.round_to_4k();

        if let Some((&start, iv)) = self.regions.range(range.as_std()).next() {
            let existing_range = Range {
                start,
                len: iv.end() - start,
            };
            printerrln!("{:?} interferes with {:?}", range, existing_range);
            #[cfg(list_regions)]
            printerrln!("regions:\n{:#x?}", self.regions);
            return false;
        }
        self.regions.insert(range.as_std(), state);
        #[cfg(debug)]
        printerrln!("added region {:?}", range);
        true
    }
    pub fn find_overlapping_region(&self, needle: Range) -> Option<MemRegion> {
        let range = needle.as_std();
        let (&start, interval_value) = self.regions.range(range).next()?;
        Some(MemRegion {
            range: Range {
                start,
                len: interval_value.end() - start,
            },
            state: *interval_value.value(),
        })
    }
    pub fn find_region_exact(&self, needle: Range) -> Option<MemRegion> {
        self.find_overlapping_region(needle).and_then(|r| {
            if r.range == needle {
                Some(r)
            } else {
                None
            }
        })
    }
    pub fn find_region_containing_addr(&self, needle: usize) -> Option<MemRegion> {
        self.find_overlapping_region(Range {
            start: needle,
            len: 0,
        })
    }
    pub fn remove_region(&mut self, needle: Range) -> Option<MemRegion> {
        self.find_region_exact(needle).map(|r| {
            let is_some = self.regions.remove(&needle.start).is_some();
            assert!(is_some);
            r
        })
    }
    pub fn all_overlapping_regions<F>(&self, needle: Range, mut predicate: F) -> bool
    where
        F: FnMut(&MemRegion) -> bool,
    {
        let range = needle.as_std();
        self.regions.range(range).all(|(&start, interval_value)| {
            let this_range = Range {
                start,
                len: *interval_value.end() - start,
            };
            let state = interval_value.value().clone();
            predicate(&MemRegion {
                range: this_range,
                state,
            })
        })
    }

    /* removes exactly the specified range from all existing regions, leaving any other parts of that region mapped */
    fn split_out_region(&mut self, mut subrange: Range) -> Result<State, usize> {
        subrange.round_to_4k();

        let mut removed_state = None;
        let mut count = 0;
        while let Some(r) = self.find_overlapping_region(subrange) {
            // remove the range containing the subrange; this should not fail
            let found = self.remove_region(r.range).is_some();
            assert!(found);

            // re-add any prefix range
            let before = Range {
                start: r.range.start,
                len: subrange.start - r.range.start,
            };
            self.add_region(before, r.state);

            // re-add any suffix range
            let after = Range {
                start: subrange.end(),
                len: r.range.end() - subrange.end(),
            };
            self.add_region(after, r.state);

            if count == 0 {
                removed_state = Some(r.state);
            }
            count += 1;
        }
        /*if !r.range.subsumes(subrange) {
            #[cfg(debug)]
            printerrln!("{:?} does not subsume {:?}", r.range, subrange);
            return None;
        }*/

        // return the split-out range's state, or the count ranges if not exactly 1
        if count == 1 {
            Ok(removed_state.unwrap())
        } else {
            Err(count)
        }
    }

    pub fn split_region(
        &mut self,
        subrange: Range,
        owner_pkey: u8,
        prot: u32,
    ) -> Option<MemRegion> {
        let state = self.split_out_region(subrange).ok()?;

        // add the new split-off range
        let new_state = State {
            owner_pkey,
            prot,
            ..state
        };
        self.add_region(subrange, new_state);
        Some(MemRegion {
            range: subrange,
            state: new_state,
        })
    }
}

#[no_mangle]
pub extern "C" fn memory_map_new() -> Box<MemoryMap> {
    Box::new(MemoryMap::new())
}

#[no_mangle]
pub extern "C" fn memory_map_destroy(_map: Box<MemoryMap>) {}

#[no_mangle]
pub extern "C" fn memory_map_mark_init_finished(map: &mut MemoryMap) -> bool {
    map.mark_init_finished()
}

#[no_mangle]
pub extern "C" fn memory_map_is_init_finished(map: &MemoryMap) -> bool {
    map.init_finished
}

#[no_mangle]
pub extern "C" fn memory_map_all_overlapping_regions_have_pkey(
    map: &MemoryMap,
    needle: Range,
    pkey: u8,
) -> bool {
    #[cfg(debug)]
    printerrln!("do all mappings in {:?} have pkey {}?", needle, pkey);
    map.all_overlapping_regions(needle, |region| {
        #[cfg(debug)]
        printerrln!(
            "does {:?} have pkey {}? {}",
            region.range,
            pkey,
            region.state.owner_pkey
        );
        pkey == region.state.owner_pkey
    })
}

#[no_mangle]
pub extern "C" fn memory_map_all_overlapping_regions_pkey_mprotected(
    map: &MemoryMap,
    needle: Range,
    pkey_mprotected: bool,
) -> bool {
    map.all_overlapping_regions(needle, |region| {
        let same = pkey_mprotected == region.state.pkey_mprotected;
        #[cfg(debug)]
        printerrln!(
            "does {:?} have pkey_mprotected=={}? {}",
            region.range,
            pkey_mprotected,
            if same { "yes" } else { "no" }
        );
        same
    })
}

#[no_mangle]
pub extern "C" fn memory_map_all_overlapping_regions_mprotected(
    map: &MemoryMap,
    needle: Range,
    mprotected: bool,
) -> bool {
    map.all_overlapping_regions(needle, |region| {
        let same = mprotected == region.state.mprotected;
        #[cfg(debug)]
        printerrln!(
            "does {:?} have mprotected=={}? {}",
            region.range,
            mprotected,
            if same { "yes" } else { "no" }
        );
        same
    })
}

#[no_mangle]
pub extern "C" fn memory_map_region_get_prot(map: &MemoryMap, needle: Range) -> u32 {
    let mut prot = None;
    let same = map.all_overlapping_regions(needle, |region| match prot {
        None => {
            prot = Some(region.state.prot);
            true
        }
        Some(prot) => prot == region.state.prot,
    });
    if same {
        prot.unwrap()
    } else {
        PROT_INDETERMINATE
    }
}

/** memory_map_region_get_prot found no or multiple protections in the given range */
pub const PROT_INDETERMINATE: u32 = 0xFFFFFFFFu32;

const PKEY_MULTIPLE: u8 = 255;
const PKEY_NONE: u8 = 254;

/// return the pkey (0-15) that covers the given range, or PKEY_MULTIPLE or
/// PKEY_NONE if the entire range is not protected by exactly one pkey
#[no_mangle]
pub extern "C" fn memory_map_region_get_owner_pkey(map: &MemoryMap, needle: Range) -> u8 {
    let mut pkey = None;
    let same = map.all_overlapping_regions(needle, |region| match pkey {
        None => {
            pkey = Some(region.state.owner_pkey);
            true
        }
        Some(pkey) => region.state.owner_pkey == pkey,
    });
    if same {
        pkey.unwrap_or(PKEY_NONE) // all same (return pkey) or no pkey
    } else {
        PKEY_MULTIPLE // multiple pkeys
    }
}

#[no_mangle]
pub extern "C" fn memory_map_unmap_region(map: &mut MemoryMap, needle: Range) -> bool {
    map.split_out_region(needle).is_ok()
}

#[no_mangle]
pub extern "C" fn memory_map_add_region(
    map: &mut MemoryMap,
    range: Range,
    owner_pkey: u8,
    prot: u32,
) -> bool {
    map.add_region(
        range,
        State {
            owner_pkey,
            mprotected: false,
            pkey_mprotected: false,
            prot,
        },
    )
}

#[no_mangle]
pub extern "C" fn memory_map_split_region(
    map: &mut MemoryMap,
    range: Range,
    owner_pkey: u8,
    prot: u32,
) -> bool {
    map.split_region(range, owner_pkey, prot).is_some()
}

#[no_mangle]
pub extern "C" fn memory_map_pkey_mprotect_region(
    map: &mut MemoryMap,
    range: Range,
    pkey: u8,
) -> bool {
    if let Some(mut state) = map.split_out_region(range).ok() {
        /* forbid pkey_mprotect of memory owned by another compartment other than 0 */
        if state.owner_pkey != pkey && state.owner_pkey != 0 {
            printerrln!(
                "refusing to pkey_mprotect memory owned by compartment {} to pkey {pkey}",
                state.owner_pkey
            );
            false
        /* forbid repeated pkey_mprotect */
        } else if state.pkey_mprotected == true {
            printerrln!("{} already pkey_mprotected region", state.owner_pkey);
            false
        /* otherwise, allow */
        } else {
            state.pkey_mprotected = true;
            /* set owner if a trusted compartment protected untrusted memory */
            if state.owner_pkey == 0 {
                state.owner_pkey = pkey;
            }
            map.add_region(range, state)
        }
    } else {
        // we're attempting to pkey_mprotect memory that was never mmapped.
        // it may have come from brk() or the initial mappings from exec().
        true
    }
}

#[no_mangle]
pub extern "C" fn memory_map_mprotect_region(map: &mut MemoryMap, range: Range, prot: u32) -> bool {
    if let Some(mut state) = map.split_out_region(range).ok() {
        if state.mprotected == false {
            state.mprotected = true;
            state.prot = prot;
            map.add_region(range, state)
        } else {
            /* after init has finished, we should warn about reprotecting regions */
            if map.init_finished {
                printerrln!(
                    "warning: reprotecting already-mprotected region {:?} (prot {} => {})",
                    range,
                    state.prot,
                    prot
                );
            }
            state.mprotected = true;
            state.prot = prot;
            map.add_region(range, state)
        }
    } else {
        // we're attempting to mprotect memory that was never mmapped.
        // it may have come from brk() or the initial mappings from exec().
        true
    }
}

#[no_mangle]
pub extern "C" fn memory_map_clear(map: &mut MemoryMap) {
    map.regions = Default::default();
    map.init_finished = false;
}

#[no_mangle]
pub extern "C" fn memory_map_clone(map: &mut MemoryMap) -> Box<MemoryMap> {
    Box::new(map.clone())
}
