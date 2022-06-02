use std::ffi::CString;

/// Arguments passed to dl_iterate_phdr while searching for the segments to
/// initialize for a given compartment key.
#[repr(C)]
pub struct PhdrSearchArgs {
    /// The compartment key to use when the segments are found
    pkey: i32,
    /// The address to search for while iterating through segments
    address: *const libc::c_void,
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
        stop: *const u8,
    }

    impl AddressRange {
        pub fn intersects(&self, other: &AddressRange) -> bool {
            self.start < other.stop && self.stop > other.start
        }
        pub fn contains(&self, other: &AddressRange) -> bool {
            self.start <= other.start && self.stop >= other.stop
        }
    }

    fn get_address_range(lib: *mut libc::c_void, start: &str, end: &str) -> AddressRange {
        let start = CString::new(start).expect("Unable to convert symbol name.");
        let start = unsafe { libc::dlsym(lib, start.as_ptr()) as *const u8 };
        let stop = CString::new(end).expect("Unable to convert symbol name.");
        let stop = unsafe { libc::dlsym(lib, stop.as_ptr()) as *const u8 };
        AddressRange { start, stop }
    }

    let lib = libc::dlopen(info.dlpi_name, libc::RTLD_NOW);
    // The __start_* and __stop_* symbols don't necessarily correspond to the start and end of the
    // sections, but the contents of the sections are contained entirely within these symbols
    let ignore_sections = ["ia2_shared_data", "ia2_shared_rodata"];
    let ignore_ranges = ignore_sections.map(|section| {
        get_address_range(
            lib,
            &("__start_".to_string() + section),
            &("__stop_".to_string() + section),
        )
    });

    // Check that the ignored ranges are page-aligned and padded
    for section in &ignore_sections {
        let start_sym = "__start_".to_string() + section;
        let stop_sym = "__stop_".to_string() + section;
        assert!(
            libc::dlsym(lib, start_sym.as_ptr().cast()).align_offset(PAGE_SIZE) == 0,
            "Start of section {:?} is not page-aligned",
            section
        );
        assert!(
            libc::dlsym(lib, stop_sym.as_ptr().cast()).align_offset(PAGE_SIZE) == 0,
            "End of section {:?} is not page-aligned",
            section
        );
    }

    let mut protected_ranges = Vec::new();

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
        let stop = (start + phdr.p_memsz) as *const u8;
        let start = start as *const u8;
        let mut mprotect_ranges = vec![AddressRange { start, stop }];
        let mut result;

        for ignore_range in &ignore_ranges {
            result = Vec::new();
            for mprotect_range in mprotect_ranges {
                if mprotect_range.intersects(&ignore_range) {
                    if ignore_range.contains(&mprotect_range) {
                        continue;
                    }
                    if mprotect_range.contains(&ignore_range) {
                        let start = mprotect_range.start;
                        let stop = ignore_range.start;
                        result.push(AddressRange { start, stop });
                        let start = ignore_range.stop;
                        let stop = mprotect_range.stop;
                        result.push(AddressRange { start, stop });
                    } else {
                        if mprotect_range.start < ignore_range.stop
                            && mprotect_range.stop >= ignore_range.stop
                        {
                            let start = ignore_range.stop;
                            let stop = mprotect_range.stop;
                            result.push(AddressRange { start, stop });
                        } else if mprotect_range.start < ignore_range.start
                            && mprotect_range.stop >= ignore_range.start
                        {
                            let start = mprotect_range.start;
                            let stop = ignore_range.start;
                            result.push(AddressRange { start, stop });
                        }
                    }
                } else {
                    result.push(mprotect_range);
                }
            }
            mprotect_ranges = result;
        }

        for range in mprotect_ranges {
            protected_ranges.push((range, prot));
        }
    }
    unsafe fn pkey_mprotect(mut start: *const u8, mut end: *const u8, prot: i32, pkey: i32) {
        if start == end {
            return;
        }
        // We checked above that all ignored (i.e. shared) sections are page-aligned and padded, so
        // we can safely move the start and end to page-boundaries
        let start_offset = start.align_offset(PAGE_SIZE);
        if start_offset != 0 {
            start = start.add(start_offset).sub(PAGE_SIZE);
        }
        let end_offset = end.align_offset(PAGE_SIZE);
        if end_offset != 0 {
            end = end.add(end_offset);
        }

        let addr = start;
        let len = end.offset_from(start);
        if len > 0 {
            libc::syscall(libc::SYS_pkey_mprotect, addr, len, prot, pkey);
        }
    }
    // After calling `pkey_mprotect` on the first phdr, we can't access `info->dlpi_phdr`
    // anymore since it's in the first phdr itself.
    for (r, prot) in protected_ranges {
        pkey_mprotect(r.start, r.stop, prot, pkey);
    }
    // Return a non-zero value to stop iterating through phdrs
    return 1;
}
