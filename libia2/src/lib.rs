#![feature(linkage)]

use core::mem::size_of;
use core::cell::Cell;
use core::ffi::c_void;
use core::sync::atomic::{AtomicI32, Ordering};

// Values for the 'trusted compartment pkey' field below.
// -2 == uninitialized
// -1 == unsupported
//  0 == ERROR (this would be the default untrusted pkey)
// >0 == trusted key
pub const PKEY_UNINITIALIZED: i32 = -2;
const MPK_UNSUPPORTED: i32 = -1;
const NUM_KEYS: usize = 15;

type CompartmentKeys = [AtomicI32; NUM_KEYS];

// The ia2_init_data section contains data which must be written during
// library initialization, and then become readable to both trusted and
// untrusted code, but not writeable from untrusted.  It may (but doesn't
// currently) remain writeable from trusted.
#[repr(align(4096))]
pub struct IA2InitDataSection {
    // Trusted Compartment PKEY. This is the value of the pkey for the trusted
    // compartment if initialized or the initialization status if not.  See
    // values defined above.
    pub pkeys: CompartmentKeys,
    _padding: [u8; PAGE_SIZE - size_of::<CompartmentKeys>()],
}

const UNINIT_PKEY: AtomicI32 = AtomicI32::new(PKEY_UNINITIALIZED);

#[link_section = "ia2_init_data"]
#[no_mangle]
pub static IA2_INIT_DATA: IA2InitDataSection = IA2InitDataSection {
    pkeys: [UNINIT_PKEY; NUM_KEYS],
    _padding: [0; PAGE_SIZE - size_of::<CompartmentKeys>()],
};

thread_local!(static THREAD_HEAP_PKEY_FLAG: Cell<[bool; NUM_KEYS]> = Cell::new([false; NUM_KEYS]));

#[repr(C)]
struct PhdrArgs {
    pkey_idx: usize,
    address: *const libc::c_void,
}

// TODO: Remove the heap arguments to initialize_heap_pkey. This is just a stopgap until we figure
// out what to do for the allocator
/// Allocates a protection key and calls `pkey_mprotect` on all pages in the trusted compartment and
/// the pages defined by `heap_start` and `heap_len`. The input heap is ignored if either argument is
/// zero.
#[no_mangle]
pub extern "C" fn initialize_compartment(pkey_idx: usize, address: *const libc::c_void) {
    THREAD_HEAP_PKEY_FLAG.with(|pkey_flags| {
        let mut flags = pkey_flags.get();
        if !flags[pkey_idx] {
            flags[pkey_idx] = true;
            pkey_flags.set(flags);

            let mut pkey = IA2_INIT_DATA.pkeys[pkey_idx].load(Ordering::Relaxed);
            if pkey == PKEY_UNINITIALIZED {
                pkey = unsafe { libc::syscall(libc::SYS_pkey_alloc, 0u32, 0u32) as i32 };
                assert!(pkey != MPK_UNSUPPORTED);
                let result = IA2_INIT_DATA.pkeys[pkey_idx].compare_exchange(
                    PKEY_UNINITIALIZED,
                    pkey,
                    Ordering::Relaxed,
                    Ordering::Relaxed,
                );
                if let Err(other_pkey) = result {
                    assert!(other_pkey != PKEY_UNINITIALIZED);
                    // Another thread beat us to it and already
                    // allocated a key, so release ours
                    unsafe {
                        libc::syscall(libc::SYS_pkey_free, pkey);
                    }
                }
            }

            unsafe {
                let mut args = PhdrArgs {
                    pkey_idx,
                    address,
                };
                // Iterate through all ELF segments and assign
                // protection keys to ours
                libc::dl_iterate_phdr(Some(phdr_callback), &mut args as *mut PhdrArgs as *mut _);

                // Now that all segmentss are setup correctly and we've done
                // the one time init (above) revoke the write protections on
                // the ia2_init_data section.
                //protect_ia2_init_data();
            }
        }
    })
}

extern "C" {
    #[linkage = "extern_weak"]
    static __start_ia2_shared_data: *const u8;
    #[linkage = "extern_weak"]
    static __stop_ia2_shared_data: *const u8;
    #[linkage = "extern_weak"]
    static __start_ia2_init_data: *const u8;
    #[linkage = "extern_weak"]
    static __stop_ia2_init_data: *const u8;
}

const PAGE_SIZE: usize = 4096;

/// Assigns the protection key to the pages in the ELF segment for the trusted compartment. Skips
/// all other segments.
unsafe extern "C" fn phdr_callback(
    info: *mut libc::dl_phdr_info,
    _size: libc::size_t,
    data: *mut libc::c_void,
) -> libc::c_int {
    let info = &*info;

    let phdr_args = &*(data as *const PhdrArgs);
    let pkey_idx = phdr_args.pkey_idx;
    let address = phdr_args.address;
    // Use the address of `_start` to determine which program headers belong to the trusted
    // compartment
    let is_trusted_compartment = (0..info.dlpi_phnum).any(|i| {
        let phdr = &*info.dlpi_phdr.add(i.into());
        if phdr.p_type == libc::PT_LOAD {
            let start = info.dlpi_addr + phdr.p_vaddr;
            let end = start + phdr.p_memsz;
            (start..end).contains(&(address as u64))
        } else {
            false
        }
    });

    if !is_trusted_compartment {
        return 0;
    }

    struct AddressRange {
        start: *const u8,
        end: *const u8,
    }

    // ia2_shared_data is the only section in the trusted compartment that we should ignore.
    let ignore_range = AddressRange {
        start: __start_ia2_shared_data,
        end: __stop_ia2_shared_data,
    };

    let pkey = IA2_INIT_DATA.pkeys[pkey_idx].load(Ordering::Relaxed);

    // Assign our secret protection key to every segment
    // in the current binary
    for i in 0..info.dlpi_phnum {
        let phdr = &*info.dlpi_phdr.add(i.into());
        if phdr.p_type != libc::PT_LOAD && phdr.p_type != libc::PT_GNU_RELRO {
            continue;
        }
        // FIXME: we don't have libc::PF_* flags
        let mut prot = libc::PROT_NONE;
        if (phdr.p_flags & 1) != 0 {
            prot |= libc::PROT_EXEC;
        }
        if (phdr.p_flags & 2) != 0 {
            prot |= libc::PROT_WRITE;
        }
        if (phdr.p_flags & 4) != 0 {
            prot |= libc::PROT_READ;
        }

        let start = info.dlpi_addr + phdr.p_vaddr;
        let end = start + phdr.p_memsz;
        let mut mprotect_range = AddressRange { start: start as *const u8, end: end as *const u8 };

        fn pkey_mprotect(start: *const u8, end: *const u8, prot: i32, pkey: i32) {
            assert!(start.align_offset(PAGE_SIZE) == 0, "Start of section at {:p} is not page-aligned", start);
            assert!(end.align_offset(PAGE_SIZE) == 0, "End of section at {:p} is not page-aligned", end);

            let addr = start;
                unsafe {
            let len = end.offset_from(start);
            if len > 0 {
                    libc::syscall(libc::SYS_pkey_mprotect, addr, len, prot, pkey);
                }
            }
        }
        // If the ignore range splits the phdr in two non-zero subsegments, we need two calls to pkey_mprotect
        let ignore_splits_phdr = ignore_range.start > mprotect_range.start
            && ignore_range.end < mprotect_range.end;
        if ignore_splits_phdr {
            // protect region before ignored range
            pkey_mprotect(mprotect_range.start, ignore_range.start, prot, pkey);

            // protect region after ignored range
            pkey_mprotect(ignore_range.end, mprotect_range.end, prot, pkey);
        } else {
            // If the ignore range overlaps the phdr we need to shift either the start or the end
            let ignore_overlaps_start = ignore_range.end > mprotect_range.start && ignore_range.start <= mprotect_range.start;
            let ignore_overlaps_end = ignore_range.start < mprotect_range.end && ignore_range.end >= mprotect_range.end;
            if ignore_overlaps_start {
                mprotect_range.start = ignore_range.end.min(mprotect_range.end);
            } else if ignore_overlaps_end {
                mprotect_range.end = ignore_range.start.max(mprotect_range.start);
            }
            pkey_mprotect(mprotect_range.start, mprotect_range.end, prot, pkey);
        }
    }
    // Return a non-zero value to stop iterating through phdrs
    return 1;
}

// Mark the memory containing the ia2_init_data section as readonly
unsafe extern "C" fn protect_ia2_init_data() {
    // Note: We carefully arranged above that the contents of this section
    // are one object of exactly page size which is page aligned.  Assert
    // that we succeeded, then mprotect.
    let start = __start_ia2_init_data as usize;
    let stop = __stop_ia2_init_data as usize;
    assert!(start % PAGE_SIZE == 0 && stop % PAGE_SIZE == 0);
    let size = stop - start;
    assert!(size % PAGE_SIZE == 0);
    let res = libc::mprotect(start as *mut c_void, size, libc::PROT_READ);
    assert!(res == 0);
}
