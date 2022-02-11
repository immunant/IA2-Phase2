#![feature(linkage)]

use core::mem::size_of;
use core::ffi::c_void;
use core::sync::atomic::{AtomicI32, Ordering};

// Values for the 'pkeys' field in IA2InitDataSection below.
// -2 == uninitialized
// -1 == unsupported
//  0 == ERROR (this would be the default untrusted pkey)
// >0 == trusted key
const PKEY_UNINITIALIZED: i32 = -2;

// On linux protection key 0 (the two LSBits of PKRU) is the default for
// anything not covered by pkey_mprotect so we only have 15 keys to work with.
const NUM_KEYS: usize = 15;

type CompartmentKeys = [AtomicI32; NUM_KEYS];

// The ia2_init_data section contains data which is written to during
// compartment initialization and then becomes read-only. Loading a library that
// initializes a new compartment will make this writeable again, but it must be
// made read-only before returning to the compartment.
#[repr(C)]
#[repr(align(4096))]
pub struct IA2InitDataSection {
    // These are the values of the compartment protection keys or
    // `PKEY_UNINITIALIZED` if not in use.
    pkeys: CompartmentKeys,
    _padding: [u8; PAGE_SIZE - size_of::<CompartmentKeys>()],
}

const UNINIT_PKEY: AtomicI32 = AtomicI32::new(PKEY_UNINITIALIZED);

#[link_section = "ia2_init_data"]
#[no_mangle]
pub static IA2_INIT_DATA: IA2InitDataSection = IA2InitDataSection {
    pkeys: [UNINIT_PKEY; NUM_KEYS],
    _padding: [0; PAGE_SIZE - size_of::<CompartmentKeys>()],
};

/// Arguments passed to dl_iterate_phdr while searching for the segments to
/// initialize for a given compartment key.
#[repr(C)]
struct PhdrSearchArgs {
    /// The index of the compartment key to use when the segments are found
    pkey_idx: usize,
    /// The address to search for while iterating through segments
    address: *const libc::c_void,
}

/// Allocates a protection key and stores it in the `pkey_idx` slot in
/// IA2_INIT_DATA. Also calls `pkey_mprotect` on all pages in the compartment
/// containing `address`. This is only used internally by INIT_COMPARTMENT in ia2.h.
#[no_mangle]
pub extern "C" fn initialize_compartment(pkey_idx: usize, address: *const libc::c_void) {
    let pkey = IA2_INIT_DATA.pkeys[pkey_idx].load(Ordering::Relaxed);

    if pkey == PKEY_UNINITIALIZED {
        // We carefully arranged above that the contents of this section are
        // one object of exactly page size which is page aligned.  Assert
        // that we succeeded, then mprotect.
        let (start, size) = unsafe {
            let start = __start_ia2_init_data as usize;
            let stop = __stop_ia2_init_data as usize;
            assert!(start % PAGE_SIZE == 0 && stop % PAGE_SIZE == 0);
            let size = stop - start;
            assert!(size % PAGE_SIZE == 0);
            (start, size)
        };

        // Change the permissions on the memory containing the ia2_init_data section.
        unsafe {
            let res = libc::mprotect(start as *mut c_void, size, libc::PROT_READ | libc::PROT_WRITE);
            assert!(res == 0);
        }

        let new_pkey = unsafe { libc::syscall(libc::SYS_pkey_alloc, 0u32, 0u32) as i32 };
        IA2_INIT_DATA.pkeys[pkey_idx].store(new_pkey, Ordering::Relaxed);

        let mut args = PhdrSearchArgs {
            pkey_idx,
            address,
        };
        // Iterate through all ELF segments and assign
        // protection keys to ours
        unsafe {
            libc::dl_iterate_phdr(Some(phdr_callback), &mut args as *mut PhdrSearchArgs as *mut _);

            // Now that all segmentss are setup correctly and we've done
            // the init (above) revoke the write protections on
            // the ia2_init_data section.
            let res = libc::mprotect(start as *mut c_void, size, libc::PROT_READ);
            assert!(res == 0);
        }
    }
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

    let phdr_args = &*(data as *const PhdrSearchArgs);
    let pkey_idx = phdr_args.pkey_idx;
    let address = phdr_args.address;
    // Use the address in PhdrSearchArgs to determine which program headers
    // belong to the compartment being initialized.
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
