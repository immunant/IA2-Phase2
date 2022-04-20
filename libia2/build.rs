use cbindgen;
use cbindgen::{Language, Style};
use std::env;

const HEADER_NAME: &str = "pkey_init.h";

fn main() {
    println!("cargo:rerun-if-changed={}", HEADER_NAME);
    let crate_dir = env::var("CARGO_MANIFEST_DIR").expect("Could not find CARGO_MANIFEST_DIR");
    cbindgen::Builder::new()
        .include_item("PhdrSearchArgs")
        .rename_item("dl_phdr_info", "struct dl_phdr_info")
        .with_after_include("struct dl_phdr_info;")
        .with_style(Style::Tag)
        .with_crate(crate_dir)
        .with_include_guard("PKEY_INIT_H")
        .with_language(Language::C)
        .with_include_version(true)
        .generate()
        .expect("Unable to generate bindings")
        .write_to_file(HEADER_NAME);
}
