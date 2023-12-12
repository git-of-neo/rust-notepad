use core::panic;
use std::process::Command;

fn build_extension<'a>(sqlite_path: &'a str, path_to_extension: &'a str, output: &'a str) {
    println!("cargo:rerun-if-changed={}", path_to_extension);
    let status = Command::new("gcc")
        .args(&[
            "-g",
            "-shared",
            "-o",
            output,
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

fn main() {
    let sqlite_path = "lib/sqlite-amalgamation-3440200";

    build_extension(sqlite_path, "ext/uuid.c", "uuid.dll");
    build_extension(sqlite_path, "ext/fuzzy.c", "fuzzy.dll");
}
