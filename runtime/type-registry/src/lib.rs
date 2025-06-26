#![cfg_attr(not(test), no_std)]

// Use the C allocator; don't use libstd.
extern crate alloc;

use alloc::collections::BTreeMap;
#[cfg(not(test))]
use crate::no_std::eprintln;
use core::fmt;
use core::fmt::Debug;
use core::fmt::Display;
use core::fmt::Formatter;
use core::ptr;
use libc_alloc::LibcAlloc;
use spin::RwLock;
#[cfg(test)]
use std::eprintln;

#[global_allocator]
static ALLOCATOR: LibcAlloc = LibcAlloc;

#[cfg(not(test))]
mod no_std {
    use core::fmt;
    use core::panic::PanicInfo;
    use libc::STDERR_FILENO;
    use libc::abort;
    use libc::write;

    pub struct StdErrWriter;

    impl fmt::Write for StdErrWriter {
        /// async-signal-safe
        fn write_str(&mut self, s: &str) -> Result<(), fmt::Error> {
            // SAFETY: `s.as_ptr()` points to `s.len()` bytes.
            unsafe { write(STDERR_FILENO, s.as_ptr().cast(), s.len()) };
            Ok(())
        }
    }

    // Print errors via libc.
    macro_rules! eprintln {
        ($($items: expr),+) => {{
            use core::fmt::Write;
            use crate::no_std::StdErrWriter;

            let _ = writeln!(&mut StdErrWriter, $($items,)+);
        }}
    }

    pub(crate) use eprintln;

    #[panic_handler]
    fn panic(info: &PanicInfo) -> ! {
        eprintln!("{info}");
        // SAFETY: `abort` is always safe.
        unsafe { abort() };
    }

    #[unsafe(no_mangle)]
    extern "C" fn rust_eh_personality() {}

    #[link(name = "gcc_s")]
    unsafe extern "C" {}
}

pub type Ptr = *const ();

/// A pointer's address without provenance, used purely as an address for comparisons.
#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub struct PtrAddr(usize);

impl Display for PtrAddr {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "ptr {:?}", ptr::without_provenance::<()>(self.0))
    }
}

impl Debug for PtrAddr {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "{}", self)
    }
}

/// A unique ID for a type.
///
/// This can be anything, as long as it's unique for a type in a program.
#[derive(Clone, Copy, PartialEq, Eq, Hash)]
#[repr(transparent)]
pub struct TypeId(u32);

impl Display for TypeId {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "type {}", self.0)
    }
}

impl Debug for TypeId {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "{}", self)
    }
}

static GLOBAL_TYPE_REGISTRY: TypeRegistry = TypeRegistry::new();

/// See [`TypeRegistry::construct`].
#[unsafe(no_mangle)]
pub extern "C-unwind" fn ia2_type_registry_construct(ptr: Ptr, type_id: TypeId) {
    GLOBAL_TYPE_REGISTRY.construct(ptr, type_id);
}

/// See [`TypeRegistry::destruct`].
#[unsafe(no_mangle)]
pub extern "C-unwind" fn ia2_type_registry_destruct(ptr: Ptr, expected_type_id: TypeId) {
    GLOBAL_TYPE_REGISTRY.destruct(ptr, expected_type_id);
}

/// See [`TypeRegistry::check`].
#[unsafe(no_mangle)]
pub extern "C-unwind" fn ia2_type_registry_check(ptr: Ptr, expected_type_id: TypeId) {
    GLOBAL_TYPE_REGISTRY.check(ptr, expected_type_id);
}

#[derive(Default)]
pub struct TypeRegistry {
    map: RwLock<BTreeMap<PtrAddr, TypeId>>,
}

impl TypeRegistry {
    pub const fn new() -> Self {
        Self {
            map: RwLock::new(BTreeMap::new()),
        }
    }

    /// Called after an object is constructed to register it and its type.
    ///
    /// Panics if `ptr` is already constructed.
    #[track_caller]
    pub fn construct(&self, ptr: Ptr, type_id: TypeId) {
        let ptr = PtrAddr(ptr.addr());
        let map = &mut *self.map.write();
        if cfg!(debug_assertions) {
            eprintln!("construct({ptr}, {type_id}): {map:#?}");
        }
        let prev_type_id = map.insert(ptr, type_id);
        if let Some(prev_type_id) = prev_type_id {
            panic!("trying to construct {type_id} at {ptr}, but {ptr} is already {prev_type_id}");
        }
    }

    /// Called after an object is destructured to unregister it and its type.
    ///
    /// Panics if `ptr` has a different type.
    #[track_caller]
    pub fn destruct(&self, ptr: Ptr, expected_type_id: TypeId) {
        let ptr = PtrAddr(ptr.addr());
        let map = &mut *self.map.write();
        if cfg!(debug_assertions) {
            eprintln!("destruct({ptr}, {expected_type_id}): {map:#?}");
        }
        let type_id = map.remove(&ptr);
        if type_id.is_none() {
            panic!(
                "trying to destruct {expected_type_id} at {ptr}, but {ptr} has no type currently"
            );
        }
    }

    /// Called to check a pointer has the expected type.
    ///
    /// Panics if `ptr` is not registered or has a different type.
    #[track_caller]
    pub fn check(&self, ptr: Ptr, expected_type_id: TypeId) {
        let ptr = PtrAddr(ptr.addr());
        let map = &*self.map.read();
        if cfg!(debug_assertions) {
            eprintln!("check({ptr}, {expected_type_id}): {map:#?}");
        }
        let type_id = map.get(&ptr).copied();
        match type_id {
            None => {
                panic!("{ptr} should have {expected_type_id}, but has no type currently");
            }
            Some(type_id) if type_id != expected_type_id => {
                panic!("{ptr} should have {expected_type_id}, but has {type_id} instead");
            }
            Some(_) => {}
        }
    }
}

impl Drop for TypeRegistry {
    fn drop(&mut self) {
        let map = &*self.map.read();
        if !map.is_empty() {
            eprintln!("warning: leak: {map:?}");
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn ptr(addr: usize) -> Ptr {
        core::ptr::without_provenance(addr)
    }

    #[test]
    fn normal() {
        let registry = TypeRegistry::default();
        registry.construct(ptr(0xA), TypeId(1));
        registry.check(ptr(0xA), TypeId(1));
        registry.construct(ptr(0xB), TypeId(2));
        registry.check(ptr(0xA), TypeId(1));
        registry.check(ptr(0xB), TypeId(2));
        registry.destruct(ptr(0xA), TypeId(1));
        registry.check(ptr(0xB), TypeId(2));
        registry.destruct(ptr(0xB), TypeId(2));
    }

    #[test]
    #[should_panic]
    fn destruct_non_existent_ptr() {
        let registry = TypeRegistry::default();
        registry.destruct(ptr(0xA), TypeId(1));
    }

    #[test]
    #[should_panic]
    fn check_non_existent_ptr() {
        let registry = TypeRegistry::default();
        registry.check(ptr(0xA), TypeId(1));
    }

    #[test]
    #[should_panic]
    fn check_freed_ptr() {
        let registry = TypeRegistry::default();
        registry.construct(ptr(0xA), TypeId(1));
        registry.check(ptr(0xA), TypeId(1));
        registry.destruct(ptr(0xA), TypeId(1));
        registry.check(ptr(0xA), TypeId(1));
    }

    #[test]
    #[should_panic]
    fn construct_existing_ptr() {
        let registry = TypeRegistry::default();
        registry.construct(ptr(0xA), TypeId(1));
        registry.check(ptr(0xA), TypeId(1));
        registry.construct(ptr(0xA), TypeId(1));
    }
}
