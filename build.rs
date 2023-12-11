use core::panic;
use std::process::Command;

fn main() {
    println!("cargo:rerun-if-changed=ext/uuid.c");

    let sqlite_path = "lib/sqlite-amalgamation-3440200";

    let status = Command::new("gcc")
        .args(&[
            "-g",
            "-shared",
            "-o",
            "uuid.dll",
            "ext/uuid.c",
            "-I",
            sqlite_path,
        ])
        .status()
        .unwrap();

    if !status.success() {
        panic!("Extension compilation failed");
    }
}
