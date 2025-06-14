#[cfg(not(test))]
pub mod panic_abort_impl {
    use std::backtrace::Backtrace;
    use std::fmt::Arguments;
    use std::panic::Location;
    use std::process::abort;
    use std::thread;

    #[cfg(not(test))]
    #[track_caller]
    pub fn panic_abort_args(args: Arguments) -> ! {
        let thread = thread::current();
        let thread_name = thread.name().unwrap_or("<unnamed>");
        let caller = Location::caller();
        let file = caller.file();
        let line = caller.line();
        let col = caller.column();
        let backtrace = Backtrace::capture();
        eprintln!("thread '{thread_name}' panicked at {file}:{line}:{col}:\n{args}\n{backtrace}");

        abort();
    }
}

#[cfg(not(test))]
macro_rules! panic_abort {
    ($($arg:tt)*) => {{
        $crate::panic::panic_abort_impl::panic_abort_args(format_args!($($arg)*));
    }};
}

#[cfg(not(test))]
pub(crate) use panic_abort;

// Tests use `#[should_panic]` and this behavior is difficult to maintain with `abort()`s instead,
// so just use normal `panic!`s when testing.
#[cfg(test)]
pub(crate) use panic as panic_abort;
