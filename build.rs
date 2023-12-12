use core::panic;
use std::path::PathBuf;
use std::process::Command;

use std::{env, fs};


fn build_extension<'a>(sqlite_path: &'a str, path_to_extension: &'a str, output: &'a str) {
    println!("cargo:rerun-if-changed={}", path_to_extension);

    let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let mut compile_path = PathBuf::from(manifest_dir.clone());
    compile_path.push("target");
    compile_path.push("ext");
    compile_path.push(output);

    let status = Command::new("gcc")
        .args(&[
            "-g",
            "-shared",
            "-o",
            &compile_path.as_path().to_string_lossy(),
            path_to_extension,
            "-I",
            sqlite_path,
        ])
        .status()
        .unwrap();

    if !status.success() {
        panic!("Extension compilation failed! : {}", path_to_extension);
    }
}

fn prep_compile_folder(){
    let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let mut extension_path = PathBuf::from(manifest_dir.clone());
    extension_path.push("target");
    extension_path.push("ext");
    fs::create_dir_all(extension_path).unwrap();
}

fn main() {
    let sqlite_path = "lib/sqlite-amalgamation-3440200";

    prep_compile_folder();
    build_extension(sqlite_path, "ext/uuid.c", "uuid.dll");
    build_extension(sqlite_path, "ext/fuzzy.c", "fuzzy.dll");
}
