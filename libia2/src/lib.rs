#![feature(asm, linkage, global_asm)]

use core::mem;
use core::ptr;
use core::fmt::Debug;
use core::convert::TryInto;
use core::cell::{Cell, RefCell};
use core::ffi::c_void;
use core::sync::atomic::{AtomicI32, Ordering};

// Import the (assembly!) definition of __libia2_scrub_registers
global_asm!(include_str!("scrub_registers.s"), options(att_syntax));

// Values for the 'trusted compartment pkey' field below.
// -2 == uninitialized
// -1 == unsupported
//  0 == ERROR (this would be the default untrusted pkey)
// >0 == trusted key
const TC_PKEY_UNINITIALIZED: i32 = -2;

// The ia2_init_data section contains data which must be written during
// library initialization, and then become readable to both trusted and
// untrusted code, but not writeable from untrusted.  It may (but doesn't
// currently) remain writeable from trusted.
#[repr(align(4096))]
struct IA2InitDataSection {
    // Trusted Compartment PKEY. This is the value of the pkey for the trusted
    // compartment if initialized or the initialization status if not.  See
    // values defined above.
    tc_pkey: AtomicI32,
    _padding: [u8; 4084],
}

#[link_section = "ia2_init_data"]
static IA2_INIT_DATA: IA2InitDataSection = IA2InitDataSection {
    tc_pkey: AtomicI32::new(TC_PKEY_UNINITIALIZED),
    _padding: [0; 4084],
};

#[cfg(feature = "insecure")]
#[inline(always)]
fn modify_pkru(untrusted: bool) -> bool {
    !untrusted
}

#[cfg(not(feature = "insecure"))]
#[inline(always)]
fn modify_pkru(untrusted: bool) -> bool {
    let tc_pkey = IA2_INIT_DATA.tc_pkey.load(Ordering::Relaxed);
    if tc_pkey == TC_PKEY_UNINITIALIZED {
        panic!("Entering untrusted code without a protection key");
    }

    let mut pkru: i32;
    unsafe {
        asm!(
            "rdpkru",
            out("eax") pkru,
            in("ecx") 0,
            out("edx") _,
        );
    }

    let pkey_mask = 3i32 << (2 * tc_pkey);
    // FIXME: do we want both bits set for was_set???
    let was_untrusted = (pkru & pkey_mask) != 0;
    if untrusted {
        pkru |= pkey_mask;
    } else {
        pkru &= !pkey_mask;
    }

    unsafe {
        asm!(
            "wrpkru",
            in("eax") pkru,
            in("ecx") 0,
            in("edx") 0,
        );
    }

    was_untrusted
}

#[no_mangle]
#[inline(always)]
pub extern "C" fn __libia2_untrusted_gate(untrusted: bool) {
    let _ = modify_pkru(untrusted);
}

// FIXME: could use a BitVec
thread_local!(static THREAD_COMPARTMENT_STACK: RefCell<Vec<bool>> = RefCell::new(Vec::new()));

/// Function that switches to the trusted compartment
/// after saving the old compartment to an internal stack.
/// To return to the old compartment, call `__libia2_untrusted_gate_pop`.
#[no_mangle]
#[inline(always)]
pub extern "C" fn __libia2_untrusted_gate_push() {
    let old_compartment = modify_pkru(false);
    THREAD_COMPARTMENT_STACK.with(|stack| {
        stack.borrow_mut().push(old_compartment);
    });
}

#[no_mangle]
#[inline(always)]
pub extern "C" fn __libia2_untrusted_gate_pop() {
    let old_compartment = THREAD_COMPARTMENT_STACK.with(|stack| {
        stack
            .borrow_mut()
            .pop()
            .expect("pop() called without push()")
    });
    let _ = modify_pkru(old_compartment);
}

thread_local!(static THREAD_HEAP_PKEY_FLAG: Cell<bool> = Cell::new(false));

fn initialize_heap_pkey() {
    THREAD_HEAP_PKEY_FLAG.with(|pkey_flag| {
        if !pkey_flag.get() {
            pkey_flag.set(true);

            let mut pkey = IA2_INIT_DATA.tc_pkey.load(Ordering::Relaxed);
            if pkey == TC_PKEY_UNINITIALIZED {
                pkey = unsafe { libc::syscall(libc::SYS_pkey_alloc, 0u32, 0u32) as i32 };
                let result = IA2_INIT_DATA.tc_pkey.compare_exchange(
                    TC_PKEY_UNINITIALIZED,
                    pkey,
                    Ordering::Relaxed,
                    Ordering::Relaxed,
                );
                if let Err(other_pkey) = result {
                    assert!(other_pkey != TC_PKEY_UNINITIALIZED);
                    // Another thread beat us to it and already
                    // allocated a key, so release ours
                    unsafe {
                        libc::syscall(libc::SYS_pkey_free, pkey);
                    }
                    pkey = other_pkey;
                }
            }

            unsafe {
                //mi_heap_set_pkey(mi_heap_get_default(), pkey);

                // Iterate through all ELF segments and assign
                // protection keys to ours
                libc::dl_iterate_phdr(
                    Some(mem::transmute(phdr_callback as usize)),
                    ptr::null_mut(),
                );

                // Now that all segmentss are setup correctly and we've done
                // the one time init (above) revoke the write protections on
                // the ia2_init_data section.
                protect_ia2_init_data();
            }
        }
    })
}

const PAGE_SIZE: usize = 4096;

#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord)]
struct PageRange {
    start_page: *const u8,
    end_page: *const u8,
}

#[inline]
fn address_page_range<T: TryInto<usize>>(start_addr: T, end_addr: T) -> PageRange
where
    <T as TryInto<usize>>::Error: Debug,
{
    let start_addr = start_addr.try_into().unwrap() as *const u8;
    let end_addr = end_addr.try_into().unwrap() as *const u8;

    unsafe {
        let start_offset = start_addr.align_offset(PAGE_SIZE);
        let start_page = if start_offset == 0 {
            start_addr
        } else {
            start_addr.add(start_offset).sub(PAGE_SIZE)
        };

        let end_offset = end_addr.align_offset(PAGE_SIZE);
        let end_page = end_addr.add(end_offset);

        PageRange {
            start_page,
            end_page,
        }
    }
}

extern "C" {
    #[linkage = "extern_weak"]
    static __start_ia2_call_gates: *const u8;
    #[linkage = "extern_weak"]
    static __stop_ia2_call_gates: *const u8;
    #[linkage = "extern_weak"]
    static __start_ia2_shared_data: *const u8;
    #[linkage = "extern_weak"]
    static __stop_ia2_shared_data: *const u8;
    #[linkage = "extern_weak"]
    static __start_ia2_init_data: *const u8;
    #[linkage = "extern_weak"]
    static __stop_ia2_init_data: *const u8;
}

unsafe extern "C" fn phdr_callback(
    info: *mut libc::dl_phdr_info,
    _size: libc::size_t,
    _data: *mut libc::c_void,
) -> libc::c_int {
    let info = &*info;

    // FIXME: this assumes that all Rust code is statically linked,
    // so this function is in the same ELF segment as the rest of the
    // Rust code
    let cb_addr = phdr_callback as usize as u64;
    let is_this_module = (0..info.dlpi_phnum).any(|i| {
        let phdr = &*info.dlpi_phdr.add(i.into());
        if phdr.p_type == libc::PT_LOAD {
            let start = info.dlpi_addr + phdr.p_vaddr;
            let end = start + phdr.p_memsz;
            (start..end).contains(&cb_addr)
        } else {
            false
        }
    });

    if !is_this_module {
        return 0;
    }

    let ignore_ranges = &mut [
        address_page_range(
            __start_ia2_call_gates as usize,
            __stop_ia2_call_gates as usize,
        ),
        address_page_range(
            __start_ia2_shared_data as usize,
            __stop_ia2_shared_data as usize,
        ),
        address_page_range(
            __start_ia2_init_data as usize,
            __stop_ia2_init_data as usize,
        ),
    ];
    ignore_ranges.sort();

    let pkey = IA2_INIT_DATA.tc_pkey.load(Ordering::Relaxed);

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
        let mut mprotect_range = address_page_range(start, end);
        for ignore_range in ignore_ranges.iter() {
            if ignore_range.end_page <= mprotect_range.start_page {
                // This ignore range is entirely before the
                // mprotect_range, skip it
                continue;
            }
            if ignore_range.start_page >= mprotect_range.end_page {
                // This ignore range is entirely after the
                // mprotect_range, stop the loop (the ignore ranges
                // are sorted, so all the later ones will be outside)
                break;
            }
            if ignore_range.start_page <= mprotect_range.start_page
                && mprotect_range.end_page <= ignore_range.end_page
            {
                // Unlikely: this ignore range covers the entire
                // remaining mprotect() call
                mprotect_range.start_page = mprotect_range.end_page;
                break;
            }

            let mprotect_len = ignore_range
                .start_page
                .offset_from(mprotect_range.start_page);
            if mprotect_len > 0 {
                libc::syscall(
                    libc::SYS_pkey_mprotect,
                    mprotect_range.start_page,
                    mprotect_len,
                    prot,
                    pkey,
                );
            }

            mprotect_range.start_page = ignore_range.end_page.min(mprotect_range.end_page);
        }

        let mprotect_len = mprotect_range
            .end_page
            .offset_from(mprotect_range.start_page);
        if mprotect_len > 0 {
            libc::syscall(
                libc::SYS_pkey_mprotect,
                mprotect_range.start_page,
                mprotect_len,
                prot,
                pkey,
            );
        }
    }
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
