use std::path::Path;
use std::path::PathBuf;
use std::env;

fn get_current_working_dir() -> std::io::Result<PathBuf> {
    env::current_dir()
}

fn main() {
    let cpp_api_path = Path::new("../libcprover-cpp");
    println!("{}", cpp_api_path.display());
    let _build = cxx_build::bridge("src/lib.rs")
        .include(cpp_api_path)
        .include(get_current_working_dir().unwrap())
        .file("src/c_api.cc")
        .flag_if_supported("-std=c++11")
        .compile("cprover-rust-api");

    println!("cargo:rerun-if-changed=src/c_api.cc");
    println!("cargo:rerun-if-changed=include/c_api.h");

    println!("cargo:rustc-link-search=native=/Users/fotiskoutoulakis/Devel/cbmc/build/lib/");
    println!("cargo:rustc-link-lib=static=goto-programs");
    println!("cargo:rustc-link-lib=static=util");
    println!("cargo:rustc-link-lib=static=langapi");
    println!("cargo:rustc-link-lib=static=ansi-c");
    println!("cargo:rustc-link-lib=static=big-int");
    println!("cargo:rustc-link-lib=static=linking");
    println!("cargo:rustc-link-lib=static=cprover-api-cpp");
}
