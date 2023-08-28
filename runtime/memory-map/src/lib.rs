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
    pub fn end(&self) -> usize {
        self.start + self.len
    }
    pub fn as_std(&self) -> core::ops::Range<usize> {
        self.start..self.end()
    }
    pub fn subsumes(&self, subrange: Range) -> bool {
        self.start <= subrange.start && self.end() >= subrange.end()
    }
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct MemRegion {
    pub range: Range,
    pub owner_pkey: u8,
}

use nonoverlapping_interval_tree as disjoint_interval_tree;

use disjoint_interval_tree::NonOverlappingIntervalTree;

pub struct MemoryMap {
    regions: NonOverlappingIntervalTree<usize, u8>,
}

impl MemoryMap {
    pub fn new() -> Self {
        MemoryMap {
            regions: Default::default(),
        }
    }
    pub fn add_region(&mut self, range: Range, pkey: u8) -> bool {
        // forbid zero-length regions
        if range.len == 0 {
            return false;
        }
        if let Some((&start, iv)) = self.regions.range(range.as_std()).next() {
            let existing_range = Range {
                start,
                len: iv.end() - start,
            };
            printerrln!("{:?} interferes with {:?}", range, existing_range);
            return false;
        }
        self.regions.insert(range.as_std(), pkey);
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
            owner_pkey: *interval_value.value(),
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
    pub fn all_overlapping_regions_have_pkey(&self, needle: Range, pkey: u8) -> bool {
        let range = needle.as_std();
        printerrln!("do all mappings in {:?} have pkey {}?", needle, pkey);
        self.regions.range(range).all(|(&start, interval_value)| {
            let this_range = Range {
                start,
                len: *interval_value.end() - start,
            };
            printerrln!(
                "does {:?} have pkey {}? {}",
                this_range,
                pkey,
                *interval_value.value()
            );
            *interval_value.value() == pkey
        })
    }
    pub fn split_region(&mut self, subrange: Range, owner_pkey: u8) -> Option<MemRegion> {
        // return None if the subrange does not overlap or is not fully contained
        let r = self.find_overlapping_region(subrange)?;
        if !r.range.subsumes(subrange) {
            return None;
        }

        // remove the range containing the subrange; this should not fail
        let found = self.remove_region(r.range).is_some();
        assert!(found);

        // re-add any prefix range
        let before = Range {
            start: r.range.start,
            len: subrange.start - r.range.start,
        };
        self.add_region(before, r.owner_pkey);

        // re-add any suffix range
        let after = Range {
            start: subrange.end(),
            len: r.range.end() - subrange.end(),
        };
        self.add_region(after, r.owner_pkey);

        // add the new split-off range
        self.add_region(subrange, owner_pkey);
        Some(MemRegion {
            range: subrange,
            owner_pkey,
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
pub extern "C" fn memory_map_all_overlapping_regions_have_pkey(
    map: &MemoryMap,
    needle: Range,
    pkey: u8,
) -> bool {
    map.all_overlapping_regions_have_pkey(needle, pkey)
}

#[no_mangle]
pub extern "C" fn memory_map_remove_region(map: &mut MemoryMap, needle: Range) -> bool {
    map.remove_region(needle).is_some()
}

#[no_mangle]
pub extern "C" fn memory_map_add_region(map: &mut MemoryMap, range: Range, owner_pkey: u8) -> bool {
    map.add_region(range, owner_pkey)
}

#[no_mangle]
pub extern "C" fn memory_map_split_region(
    map: &mut MemoryMap,
    range: Range,
    owner_pkey: u8,
) -> bool {
    map.split_region(range, owner_pkey).is_some()
}
