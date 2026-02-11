#![no_std]
/* use the C allocator; don't use libstd */
extern crate alloc;
use alloc::boxed::Box;
use libc_alloc::LibcAlloc;

#[global_allocator]
static ALLOCATOR: LibcAlloc = LibcAlloc;

/* we only support panic = abort; define dummy handler and EH personality */
#[cfg(not(test))]
#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}

#[cfg(not(test))]
#[no_mangle]
extern "C" fn rust_eh_personality() {}

#[cfg(not(test))]
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

impl From<core::ops::Range<usize>> for Range {
    fn from(other: core::ops::Range<usize>) -> Self {
        Self::from_bounds(other.start, other.end).unwrap()
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

#[test]
fn test_subtract() {
    let full = Range::from_bounds(0, 100).unwrap();
    let inner = Range::from_bounds(40, 60).unwrap();
    let before = Range::from_bounds(0, 40).unwrap();
    let after = Range::from_bounds(60, 100).unwrap();

    assert_eq!(full.subtract(&inner), Some((Some(before), Some(after))));
    assert_eq!(inner.subtract(&full), Some((None, None)));

    assert_eq!(full.subtract(&full), Some((None, None)));

    assert_eq!(before.subtract(&after), None);
    assert_eq!(after.subtract(&before), None);
}

/// The state of a contiguous region of memory
#[repr(C)]
#[derive(Copy, Clone)]
pub struct State {
    pub owner_pkey: u8,
    pub pkey_mprotected: bool,
    pub mprotected: bool,
    pub prot: u32,
}

impl fmt::Debug for State {
    fn fmt(&self, fmt: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            fmt,
            "C{} [{}] ",
            self.owner_pkey,
            if self.pkey_mprotected { 'P' } else { ' ' }
        )?;
        if self.prot > 7 {
            write!(fmt, "{:03x}", self.prot)?;
        } else {
            write!(fmt, "{}", if self.prot & 1 != 0 { 'r' } else { '-' })?;
            write!(fmt, "{}", if self.prot & 2 != 0 { 'w' } else { '-' })?;
            write!(fmt, "{}", if self.prot & 4 != 0 { 'x' } else { '-' })?;
        }
        if self.mprotected {
            write!(fmt, "*")?;
        }
        Ok(())
    }
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
    fn dump(&self) {
        for (&start, state) in self.regions.range(0..usize::MAX) {
            let range = Range {
                start,
                len: state.end() - start,
            };
            printerrln!("{range:?}: {:?}", state.value());
        }
    }
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
    /// Add a region that does not overlap any existing regions.
    ///
    /// Returns whether successful.
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
        let in_position = self.regions.range(range.as_std());
        assert!(in_position.count() == 0);
        self.regions.insert(range.as_std(), state);
        #[cfg(debug)]
        printerrln!("added region {:?}", range);
        true
    }
    /// Find a region overlapping the given range.
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
    /// Find a region exactly located at the given range, if present.
    pub fn find_region_exact(&self, needle: Range) -> Option<MemRegion> {
        self.find_overlapping_region(needle).and_then(|r| {
            if r.range == needle {
                Some(r)
            } else {
                None
            }
        })
    }
    /// Find a region containing the given single address.
    pub fn find_region_containing_addr(&self, needle: usize) -> Option<MemRegion> {
        self.find_overlapping_region(Range {
            start: needle,
            len: 1,
        })
    }
    /// Remove a region exactly located at the given range.
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

    pub fn contains_holes(&self, needle: Range) -> bool {
        // if start point is not in a region, a hole exists at its start
        if self.find_region_containing_addr(needle.start).is_none() {
            return true;
        }
        let mut last_end = None;
        let all_contained_regions_contiguous = self.all_overlapping_regions(needle, |region| {
            if let Some(last_end) = last_end {
                if region.range.start != last_end {
                    return false;
                }
            }
            last_end = Some(region.range.end());
            true
        });
        if !all_contained_regions_contiguous {
            return true;
        }
        match last_end {
            // if end of final overlap is before end of range, hole at end
            Some(last_end) => last_end < needle.end(),
            // if end of final overlap is missing, hole at end
            None => true,
        }
    }

    /* removes exactly the specified range from all existing regions, leaving any other parts of that region mapped */
    fn split_out_region(&mut self, mut subrange: Range) -> alloc::vec::Vec<MemRegion> {
        subrange.round_to_4k();

        let overlapped_ranges: alloc::vec::Vec<_> = self
            .regions
            .range(subrange.as_std())
            .map(|(start, value)| (*start..*value.end(), *value.value()))
            .collect();
        for (range, _state) in &overlapped_ranges {
            self.regions.remove(&range.start).unwrap();
        }

        for (overlapped, state) in overlapped_ranges.clone() {
            match Range::from(overlapped).subtract(&subrange) {
                None => panic!("region due to overlapping does not overlap"),
                Some((before, after)) => {
                    if let Some(before) = before {
                        self.regions.insert(before.as_std(), state);
                    }
                    if let Some(after) = after {
                        self.regions.insert(after.as_std(), state);
                    }
                }
            }
        }
        overlapped_ranges
            .into_iter()
            .filter_map(|(overlapped, state)| {
                let start = overlapped.start.max(subrange.start);
                let end = overlapped.end.min(subrange.end());
                if start != end {
                    Some(MemRegion {
                        range: Range::from_bounds(start, end).unwrap(),
                        state,
                    })
                } else {
                    None
                }
            })
            .collect()
    }
}

#[test]
fn test_split_out() {
    let mut map = MemoryMap::new();
    for (start, len) in [
        (0x200000, 0x400000),
        (0x7fe000, 0x7fd000),
        (0xffb000, 0x001000),
        (0xffc000, 0x003000),
        (0xfff000, 0x7fd000),
    ] {
        map.add_region(
            Range { start, len },
            State {
                owner_pkey: 0,
                pkey_mprotected: false,
                mprotected: false,
                prot: 0,
            },
        );
    }

    let to_remove = Range {
        start: 0x7fe000,
        len: 0x801000,
    };

    let overlapping_ranges = map.regions.range(to_remove.as_std());
    assert!(overlapping_ranges.count() == 3);

    let dummy_state = State {
        owner_pkey: 255,
        pkey_mprotected: false,
        mprotected: false,
        prot: u32::MAX,
    };
    let overlapped_ranges = map
        .regions
        .clone()
        .insert_replace(to_remove.as_std(), dummy_state);
    assert!(
        overlapped_ranges.len() != 3,
        "this should be 3, but `insert_replace` is buggy"
    );

    let mut map2 = MemoryMap::new();
    for (start, len) in [(0x200000, 0x400000), (0xfff000, 0x7fd000)] {
        map2.add_region(
            Range { start, len },
            State {
                owner_pkey: 0,
                pkey_mprotected: false,
                mprotected: false,
                prot: 0,
            },
        );
    }

    let ranges = map.split_out_region(to_remove);
    assert!(ranges.len() == 3);

    assert!(map
        .regions
        .iter()
        .zip(map2.regions.iter())
        .all(|(r1, r2)| r1.0 == r2.0 && r1.1.end() == r2.1.end()));
}

#[test]
fn test_find_addr() {
    let mut map = MemoryMap::new();
    let addr = 0x7ffff7a2000;
    map.add_region(
        Range {
            start: addr,
            len: 0x2000,
        },
        State {
            owner_pkey: 0,
            pkey_mprotected: false,
            mprotected: false,
            prot: 0,
        },
    );
    assert!(map.find_region_containing_addr(addr).is_some())
}

#[no_mangle]
pub extern "C" fn memory_map_new() -> Box<MemoryMap> {
    Box::new(MemoryMap::new())
}

#[no_mangle]
pub extern "C" fn memory_map_destroy(_map: Box<MemoryMap>) {}

#[no_mangle]
pub extern "C" fn memory_map_dump(map: &mut MemoryMap) {
    map.dump()
}

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
pub extern "C" fn memory_map_unmap_region(map: &mut MemoryMap, needle: Range) {
    map.split_out_region(needle);
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
pub extern "C" fn memory_map_pkey_mprotect_region(
    map: &mut MemoryMap,
    range: Range,
    pkey: u8,
) -> bool {
    // validity: all pages of the range must be covered by entries in the map
    if map.contains_holes(range) {
        printerrln!("attempting to pkey_mprotect unmapped memory");
        return false;
    }
    // monotonicity: cannot pkey_mprotect memory twice or if owned by another compartment
    if !map.all_overlapping_regions(range, |region| {
        /* forbid pkey_mprotect of memory owned by another compartment other than 0 */
        if region.state.owner_pkey != pkey && region.state.owner_pkey != 0 {
            if map.init_finished {
                printerrln!(
                    "refusing to pkey_mprotect memory owned by compartment {} to pkey {pkey}",
                    region.state.owner_pkey
                );
                false
            } else {
                /* During init, allow re-protecting memory from one pkey to another.
                 * This is needed because the custom glibc's __minimal_malloc protects
                 * its entire heap (including TLS blocks) with IA2_LDSO_PKEY. IA2 init
                 * must re-protect each compartment's TLS with the correct pkey. */
                true
            }
        /* forbid repeated pkey_mprotect */
        } else if region.state.pkey_mprotected == true {
            if map.init_finished {
                printerrln!(
                    "refusing to re-pkey_mprotect memory already pkey_mprotected by compartment {}",
                    region.state.owner_pkey
                );
                false
            } else {
                /* During init, allow re-pkey_mprotect (same reason as above). */
                true
            }
        /* otherwise, allow */
        } else {
            true
        }
    }) {
        return false;
    }
    // update every region in overlap
    for mut region in map.split_out_region(range) {
        region.state.pkey_mprotected = true;
        /* set owner if a trusted compartment protected untrusted memory,
         * or if we are still in init (re-protecting loader heap regions). */
        if region.state.owner_pkey == 0 || !map.init_finished {
            region.state.owner_pkey = pkey;
        }
        assert!(map.add_region(region.range, region.state))
    }
    true
}

#[no_mangle]
pub extern "C" fn memory_map_mprotect_region(map: &mut MemoryMap, range: Range, prot: u32) -> bool {
    // validity: all pages of the range must be covered by entries in the map
    if map.contains_holes(range) {
        printerrln!("attempting to mprotect unmapped memory");
        return false;
    }
    // update every region in overlap
    for mut region in map.split_out_region(range) {
        if region.state.mprotected == false {
            region.state.mprotected = true;
            region.state.prot = prot;
        } else {
            /* after init has finished, we should warn about reprotecting regions */
            if map.init_finished {
                printerrln!(
                    "warning: reprotecting already-mprotected region {:?} (prot {} => {})",
                    region.range,
                    region.state.prot,
                    prot
                );
            }
            region.state.mprotected = true;
            region.state.prot = prot;
        };
        assert!(map.add_region(region.range, region.state))
    }
    true
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
