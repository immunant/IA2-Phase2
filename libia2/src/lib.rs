#![feature(
    alloc_shared,
    allocator_api,
    alloc_layout_extra,
    asm,
    core_intrinsics,
    ia2_call_gate,
    linkage,
    min_specialization,
    new_uninit,
    nonnull_slice_from_raw_parts,
    global_asm,
    avx512_target_feature
)]

// FIXME(pl): export this module so other crates can access the global variable
// for the compartment allocator and test whether a given allocation is shared.
pub mod alloc;


#[macro_use]
extern crate lazy_static;

#[cfg(test)]
mod tests {
    use crate::alloc::A as allocator;
    use std::intrinsics::compartment;
    use untrusted_test_ffi::*;

    use nix::sys::signal::Signal;
    use nix::sys::wait::{waitpid, WaitStatus};
    use nix::unistd::{fork, ForkResult};

    fn fork_it<F: FnOnce() -> ()>(test: F) -> Option<WaitStatus> {
        match unsafe { fork() } {
            Ok(ForkResult::Parent { child, .. }) =>
            // the actual result
            {
                waitpid(Some(child), None).ok()
            }
            Ok(ForkResult::Child) => {
                test();
                std::process::exit(0);
                unreachable!()
            }
            Err(_) => panic!("Fork failed"),
        }
    }

    fn expect_sigsegv<F: FnOnce() -> ()>(test: F) {
        match fork_it(test).unwrap() {
            WaitStatus::Signaled(_, Signal::SIGSEGV, _) => (), // expected
            WaitStatus::Signaled(_, Signal::SIGILL, _) => {
                let why = "Child process was killed by SIGILL. \
                                Run test on CPU with PKRU support";
                panic!(why)
            }
            WaitStatus::Signaled(_, signal, _) => panic!(
                "Expected child process to be killed by unexpected signal: {}",
                signal
            ),
            _ => panic!("Expected child process to be killed by signal."),
        }
    }

    fn expect_success<F: FnOnce() -> ()>(test: F) {
        match fork_it(test).unwrap() {
            WaitStatus::Exited(_pid, 0) => (), // expected
            WaitStatus::Exited(_pid, status) => panic!(
                "Expected child to exit with status 0. Actual status {}",
                status
            ),
            WaitStatus::Signaled(_, Signal::SIGILL, _) => {
                let why = "Child process was killed by SIGILL. \
                                        Run test on CPU with PKRU support";
                panic!(why)
            }
            WaitStatus::Signaled(_, Signal::SIGSEGV, _) => {
                let why = "Child process was killed by SIGSEGV.";
                panic!(why)
            }
            _ => panic!("Unexpected child process behavior."),
        }
    }

    // Access the provided shared value from trusted compartment
    fn trusted_shared_read(shared: &SharedValue) {
        println!("{}", shared.0);
    }
    // Access the provided secrete value from untrusted compartment
    fn trusted_secret_read(secret: &SecretValue) {
        println!("{}", secret.0);
    }

    #[test]
    fn compartment_intrinsic() {
        let secret_cmp = compartment::<SecretValue>();
        let shared_cmp = compartment::<SharedValue>();
        assert_eq!(secret_cmp, 0);
        assert_eq!(shared_cmp, 1);
    }

    #[test]
    fn simple_allocation() {
        let secret = Box::new(SecretValue(1));
        let shared = Box::new(SharedValue(1));

        assert_eq!(allocator.is_shared_ptr(&*shared), true);
        assert_eq!(allocator.is_shared_ptr(&*secret), false);
    }

    #[test]
    fn touch_shared_heap() {
        // For shared, both trusted and untrusted access okay.
        let shared = Box::new(SharedValue(1));

        assert_eq!(allocator.is_shared_ptr(&*shared), true);

        expect_success(|| unsafe {
            trusted_shared_read(&*shared);
            untrusted_shared_read(&*shared);
        });
    }

    #[test]
    fn touch_secret_heap() {
        // For a secret value, trusted access should succeed, untrusted
        // should fail.
        let secret = Box::new(SecretValue(1));

        assert_eq!(!allocator.is_shared_ptr(&*secret), true);

        expect_success(|| {
            trusted_secret_read(&*secret);
        });
        expect_sigsegv(|| unsafe {
            untrusted_secret_read(&*secret);
        });
    }


    #[test]
    fn vec_allocation() {
        let mut secret = vec![SecretValue(1), SecretValue(2)];
        let mut shared = vec![SharedValue(3), SharedValue(4)];
        dbg!(shared.as_ptr());
        dbg!(secret.as_ptr());

        assert_eq!(allocator.is_shared_ptr(shared.as_ptr()), true);
        assert_eq!(allocator.is_shared_ptr(secret.as_ptr()), false);

        secret.resize(32, SecretValue(5));
        shared.resize(32, SharedValue(6));
        dbg!(shared.as_ptr());
        dbg!(secret.as_ptr());

        assert_eq!(allocator.is_shared_ptr(shared.as_ptr()), true);
        assert_eq!(allocator.is_shared_ptr(secret.as_ptr()), false);
    }

    #[test]
    fn maybe_uninit() {
        use std::mem::MaybeUninit;

        assert_eq!(compartment::<MaybeUninit<SecretValue>>(), 0);
        assert_eq!(compartment::<MaybeUninit<SharedValue>>(), 1);

        let secret_box_uninit = Box::<SecretValue>::new_uninit();
        let shared_box_uninit = Box::<SharedValue>::new_uninit();

        assert_eq!(
            allocator.is_shared_ptr(Box::into_raw(secret_box_uninit)),
            false
        );
        assert_eq!(
            allocator.is_shared_ptr(Box::into_raw(shared_box_uninit)),
            true
        );

        let secret_box_zeroed = Box::<SecretValue>::new_zeroed();
        let shared_box_zeroed = Box::<SharedValue>::new_zeroed();

        assert_eq!(
            allocator.is_shared_ptr(Box::into_raw(secret_box_zeroed)),
            false
        );
        assert_eq!(
            allocator.is_shared_ptr(Box::into_raw(shared_box_zeroed)),
            true
        );
    }

    #[test]
    fn indirect_call() {
        #[inline(never)]
        fn call_indirect(
            f: unsafe extern "C" fn(&SecretValue, &SharedValue, bool),
            secret: &SecretValue,
            shared: &SharedValue,
            deref_secret: bool,
        ) {
            unsafe {
                f(&*secret, &*shared, deref_secret);
            }
        }

        let secret = Box::new(SecretValue(2));
        let shared = Box::new(SharedValue(2));

        expect_success(|| call_indirect(untrusted_foreign_function, &secret, &shared, false));

        expect_sigsegv(|| call_indirect(untrusted_foreign_function, &secret, &shared, true));
    }

    #[test]
    fn reverse_indirect_call() {
        #[ia2_call_gate]
        extern "C" fn callback(secret: &SecretValue) {
            dbg!(secret);
        }

        #[inline(never)]
        fn call_indirect(
            f: unsafe extern "C" fn(&SecretValue, &SharedValue, extern "C" fn(&SecretValue), bool),
            secret: &SecretValue,
            shared: &SharedValue,
            deref_secret: bool,
        ) {
            unsafe {
                f(&*secret, &*shared, callback, deref_secret);
            }
        }

        let secret = Box::new(SecretValue(3));
        let shared = Box::new(SharedValue(3));

        expect_success(|| {
            call_indirect(
                untrusted_foreign_function_with_callback,
                &secret,
                &shared,
                false,
            )
        });

        expect_sigsegv(|| {
            call_indirect(
                untrusted_foreign_function_with_callback,
                &secret,
                &shared,
                true,
            )
        });
    }

    // NOTE: This test currently fails sometimes.
    #[test]
    fn shared_static() {
        static SHARED: SharedValue = SharedValue(1);

        expect_success(|| unsafe {
            trusted_shared_read(&SHARED);
            untrusted_shared_read(&SHARED);
        });
    }

    #[test]
    fn static_box_of_shared() {
        // In this case, the static itself doesn't need to be dereferenceable
        // only the boxed contents do.
        lazy_static! {
            static ref SHARED: Box<SharedValue> = Box::new(SharedValue(1));
        }
        expect_success(|| unsafe {
            trusted_shared_read(&*SHARED);
            untrusted_shared_read(&*SHARED);
        });
    }

    #[test]
    fn secret_static() {
        static SECRET: SecretValue = SecretValue(1);

        expect_success(|| {
            trusted_secret_read(&SECRET);
        });
        expect_sigsegv(|| unsafe {
            untrusted_secret_read(&SECRET);
        });
    }

}
