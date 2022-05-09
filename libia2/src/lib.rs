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
    let ignore_ranges: [AddressRange; 8] = [
        get_address_range(lib, "__start_ia2_shared_data", "__stop_ia2_shared_data"),
        get_address_range(lib, "__start_ia2_shared_rodata", "__stop_ia2_shared_rodata"),
        get_address_range(lib, "__start_dynamic", "__stop_dynamic_padding"),
        get_address_range(lib, "__start_gnu_hash", "__stop_gnu_hash_padding"),
        get_address_range(lib, "__start_dynsym", "__stop_dynsym_padding"),
        get_address_range(lib, "__start_dynstr", "__stop_dynstr_padding"),
        get_address_range(lib, "__start_gnu_version", "__stop_gnu_version_padding"),
        get_address_range(lib, "__start_interp", "__stop_interp_padding"),
    ];

    let mut segments = Vec::new();
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
        segments.push((AddressRange { start, stop }, prot));
    }

    // Assign our secret protection key to every segment
    // in the current binary
    for segment in segments {
        let mut mprotect_ranges = vec![segment.0];
        let prot = segment.1;
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
            pkey_mprotect(range.start, range.stop, prot, pkey);
        }

        fn pkey_mprotect(start: *const u8, end: *const u8, prot: i32, pkey: i32) {
            if start == end {
                return;
            }
            assert!(
                start.align_offset(PAGE_SIZE) == 0,
                "Start of section at {:p} is not page-aligned",
                start
            );
            assert!(
                end.align_offset(PAGE_SIZE) == 0,
                "End of section at {:p} is not page-aligned",
                end
            );

            let addr = start;
            unsafe {
                let len = end.offset_from(start);
                if len > 0 {
                    libc::syscall(libc::SYS_pkey_mprotect, addr, len, prot, pkey);
                }
            }
        }
    }
    // Return a non-zero value to stop iterating through phdrs
    return 1;
}
