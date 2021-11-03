use cbindgen;
use cbindgen::Language;
use std::env;

const HEADER_NAME: &str = "../include/call_gates.h";

fn main() {
    println!("cargo:rerun-if-changed={}", HEADER_NAME);
    let crate_dir = env::var("CARGO_MANIFEST_DIR").expect("Could not find CARGO_MANIFEST_DIR");
    cbindgen::Builder::new()
        .with_crate(crate_dir)
        .with_include_guard("CALL_GATES_H")
        .with_language(Language::C)
        .generate()
        .expect("Unable to generate bindings")
        .write_to_file(HEADER_NAME);
}
