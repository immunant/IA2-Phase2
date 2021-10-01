#![feature(alloc_shared, min_specialization)]

use std::marker::Shared;

#[derive(Clone, Debug)]
#[repr(transparent)]
pub struct SecretValue(pub i32);

#[derive(Clone, Debug)]
#[repr(transparent)]
pub struct SharedValue(pub i32);

impl Shared for SharedValue {}

/// Simulates an FFI function that takes a pointer to a shared value
// TODO: need a compiler pass or linter that verifies that this interface
// only contains types that impl Shared
extern "C" {
    // Access the provided shared value from untrusted compartment
    pub fn untrusted_shared_read(shared: &SharedValue);
    // Access the provided secrete value from untrusted compartment
    // (Will crash when pkru protections active)
    pub fn untrusted_secret_read(secret: &SecretValue);

    // Unconditional read the shared value, and read the secret one if
    // the last parameter is true.
    pub fn untrusted_foreign_function(
        secret: &SecretValue,
        shared: &SharedValue,
        deref_secret: bool,
    );
    pub fn untrusted_foreign_function_with_callback(
        secret: &SecretValue,
        shared: &SharedValue,
        cb: extern "C" fn(&SecretValue),
        deref_secret: bool,
    );

    // Used for benchmarks
    pub fn untrusted_nop_function(x: i32) -> i32;
    pub fn untrusted_nop_function2(x: i32) -> i32;
    pub fn untrusted_nop_function3(x: i32) -> i32;
    pub fn untrusted_nop_function4(x: i32) -> i32;
}
