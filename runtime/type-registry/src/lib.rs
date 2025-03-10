use std::collections::HashMap;
use std::fmt;
use std::fmt::Debug;
use std::fmt::Display;
use std::fmt::Formatter;
use std::ptr;
use std::sync::LazyLock;
use std::sync::RwLock;

pub type Ptr = *const ();

/// A pointer's address without provenance, used purely as an address for comparisons.
#[derive(Clone, Copy, PartialEq, Eq, Hash)]
pub struct PtrAddr(usize);

impl Display for PtrAddr {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "{:?}", ptr::without_provenance::<()>(self.0))
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
        write!(f, "Type{}", self.0)
    }
}

impl Debug for TypeId {
    fn fmt(&self, f: &mut Formatter) -> fmt::Result {
        write!(f, "{}", self)
    }
}

static GLOBAL_TYPE_REGISTRY: LazyLock<TypeRegistry> = LazyLock::new(TypeRegistry::default);

/// See [`TypeRegistry::construct`].
#[no_mangle]
pub extern "C-unwind" fn ia2_type_registry_construct(ptr: Ptr, type_id: TypeId) {
    GLOBAL_TYPE_REGISTRY.construct(ptr, type_id);
}

/// See [`TypeRegistry::destruct`].
#[no_mangle]
pub extern "C-unwind" fn ia2_type_registry_destruct(ptr: Ptr, expected_type_id: TypeId) {
    GLOBAL_TYPE_REGISTRY.destruct(ptr, expected_type_id);
}

/// See [`TypeRegistry::check`].
#[no_mangle]
pub extern "C-unwind" fn ia2_type_registry_check(ptr: Ptr, expected_type_id: TypeId) {
    GLOBAL_TYPE_REGISTRY.check(ptr, expected_type_id);
}

#[derive(Default)]
pub struct TypeRegistry {
    map: RwLock<HashMap<PtrAddr, TypeId>>,
}

impl TypeRegistry {
    /// Called after an object is constructed to register it and its type.
    ///
    /// Panics if `ptr` is already constructed.
    #[track_caller]
    pub fn construct(&self, ptr: Ptr, type_id: TypeId) {
        let ptr = PtrAddr(ptr.addr());
        let guard = &mut *self.map.write().unwrap();
        let prev_type_id = guard.insert(ptr, type_id);
        assert_eq!(prev_type_id, None);
    }

    /// Called after an object is destructured to unregister it and its type.
    ///
    /// Panics if `ptr` has a different type.
    #[track_caller]
    pub fn destruct(&self, ptr: Ptr, expected_type_id: TypeId) {
        let ptr = PtrAddr(ptr.addr());
        let guard = &mut *self.map.write().unwrap();
        let type_id = guard.remove(&ptr);
        assert_eq!(type_id, Some(expected_type_id));
    }

    /// Called to check a pointer has the expected type.
    ///
    /// Panics if `ptr` is not registered or has a different type.
    #[track_caller]
    pub fn check(&self, ptr: Ptr, expected_type_id: TypeId) {
        let ptr = PtrAddr(ptr.addr());
        let guard = &*self.map.read().unwrap();
        let type_id = guard.get(&ptr).copied();
        assert_eq!(type_id, Some(expected_type_id));
    }
}

impl Drop for TypeRegistry {
    fn drop(&mut self) {
        match self.map.get_mut() {
            Ok(map) => {
                if !map.is_empty() {
                    eprintln!("warning: leak: {map:?}");
                }
            }
            Err(e) => {
                eprintln!("poison: {e}")
            }
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
