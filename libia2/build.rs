use std::env;
use cbindgen;
use cbindgen::Language;

const HEADER_NAME: &str = "ia2.h";

fn main() {
    println!("cargo:rerun-if-changed={}", HEADER_NAME);
    let crate_dir = env::var("CARGO_MANIFEST_DIR").expect("Could not find CARGO_MANIFEST_DIR");
    cbindgen::Builder::new()
        .with_crate(crate_dir)
        .with_language(Language::C)
        .generate()
        .expect("Unable to generate bindings")
        .write_to_file(HEADER_NAME);
}
