use std::env;

fn main() {
    let out_dir = env::var("OUT_DIR").unwrap();
    cc::Build::new()
        .shared_flag(true)
        .get_compiler()
        .to_command()
        .args(&[
            "src/solib.c",
            "-o",
            &format!("{}/libuntrusted_test_solib.so", out_dir),
            "-Wl,-z,now",
        ])
        .status()
        .expect("failed to run compiler");

    println!("cargo:rustc-link-search=native={}", out_dir);
    println!("cargo:rustc-link-lib=dylib=untrusted_test_solib");
    println!("cargo:rerun-if-changed=src/solib.c");
}
