#![feature(linkage)]

/// Arguments passed to dl_iterate_phdr while searching for the segments to
/// initialize for a given compartment key.
#[repr(C)]
pub struct PhdrSearchArgs {
    /// The compartment key to use when the segments are found
    pkey: i32,
    /// The address to search for while iterating through segments
    address: *const libc::c_void,
}

extern "C" {
    #[linkage = "extern_weak"]
    static __start_ia2_shared_data: *const u8;
    #[linkage = "extern_weak"]
    static __stop_ia2_shared_data: *const u8;
}

const PAGE_SIZE: usize = 4096;

/// Assigns the protection key to the pages in the ELF segment for the trusted compartment. Skips
/// all other segments.
#[no_mangle]
pub unsafe extern "C" fn protect_pages(
    info: *mut libc::dl_phdr_info,
    _size: libc::size_t,
    data: *mut libc::c_void,
) -> libc::c_int {
    let info = &*info;

    let phdr_args = &*(data as *const PhdrSearchArgs);
    let pkey = phdr_args.pkey;
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
