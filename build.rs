use core::panic;
use std::path::PathBuf;
use std::process::Command;

use std::{env, fs};

fn prep_compile_folder(target_path: &PathBuf) {
    fs::create_dir_all(target_path).unwrap();
}

fn compile_uuid<'a>(src_path: &PathBuf, target_path: &PathBuf, sqlite_path: &'a str) {
    let mut dll = target_path.clone();
    dll.push("uuid.dll");

    let mut c_extension = src_path.clone();
    c_extension.push("uuid");
    c_extension.push("extension.c");

    let status = Command::new("gcc")
        .args(&[
            "-g",
            "-shared",
            "-o",
            &dll.as_path().to_string_lossy(),
            &c_extension.as_path().to_string_lossy(),
            "-I",
            sqlite_path,
        ])
        .status()
        .unwrap();

    if !status.success() {
        panic!("Failed to compile uuid extension!");
    }
}

fn compile_fuzzy<'a>(src_path: &PathBuf, target_path: &PathBuf, sqlite_path: &'a str) {
    let mut src_path = src_path.clone();
    src_path.push("fuzzy");

    let mut dll = target_path.clone();
    dll.push("fuzzy.dll");

    let mut c_fuzzy = src_path.clone();
    c_fuzzy.push("fuzzySorter.c");

    let mut c_extension = src_path.clone();
    c_extension.push("extension.c");

    let status = Command::new("gcc")
        .args(&[
            "-g",
            "-shared",
            "-o",
            &dll.as_path().to_string_lossy(),
            &c_fuzzy.as_path().to_string_lossy(),
            &c_extension.as_path().to_string_lossy(),
            "-I",
            sqlite_path,
        ])
        .status()
        .unwrap();

    if !status.success() {
        panic!("Failed to compile fuzzy extension!");
    }
}

fn main() {
    // println!("cargo:rerun-if-changed=ext/");
    let sqlite_path = "lib/sqlite-amalgamation-3440200";

    let manifest_dir = env::var("CARGO_MANIFEST_DIR").unwrap();
    let mut target_path = PathBuf::from(manifest_dir.clone());
    target_path.push("target");
    target_path.push("ext");

    let mut src_path = PathBuf::from(manifest_dir.clone());
    src_path.push("ext");

    prep_compile_folder(&target_path);
    compile_uuid(&src_path, &target_path, sqlite_path);
    compile_fuzzy(&src_path, &target_path, sqlite_path);
}
