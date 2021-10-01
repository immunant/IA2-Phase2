extern crate ia2;

use criterion::{black_box, criterion_group, criterion_main, BatchSize, Criterion};
use rand::{thread_rng, Rng};
use untrusted_test_ffi::*;

fn direct_call(c: &mut Criterion) {
    c.bench_function("direct call", |b| {
        b.iter(|| unsafe { untrusted_nop_function(black_box(42)) })
    });
}

fn indirect_call(c: &mut Criterion) {
    // For every benchmark run, pick 1 of 4 targets randomly
    // to significantly reduce the impact of the branch predictor
    // (it should reduce the hit rate from 100% down to 25%)
    const UNTRUSTED_FNS: [unsafe extern "C" fn(i32) -> i32; 4] = [
        untrusted_nop_function,
        untrusted_nop_function2,
        untrusted_nop_function3,
        untrusted_nop_function4,
    ];

    let mut rng = thread_rng();
    c.bench_function("indirect call", |b| {
        b.iter_batched(
            || rng.gen_range(0..UNTRUSTED_FNS.len()),
            |idx| unsafe {
                let f = UNTRUSTED_FNS[idx];
                f(black_box(42))
            },
            BatchSize::SmallInput,
        )
    });
}

criterion_group!(benches, direct_call, indirect_call);
criterion_main!(benches);
