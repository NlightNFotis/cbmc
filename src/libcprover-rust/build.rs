use std::env;
use std::io::{Error, ErrorKind};
use std::path::Path;
use std::path::PathBuf;

fn get_current_working_dir() -> std::io::Result<PathBuf> {
    env::current_dir()
}

fn get_library_build_dir() -> std::io::Result<PathBuf> {
    let env_var_name = "CBMC_LIB_DIR";
    let env_var_fetch_result = env::var(env_var_name);
    if let Ok(lib_dir) = env_var_fetch_result {
        let mut path = PathBuf::new();
        path.push(lib_dir);
        return Ok(path);
    }
    Err(Error::new(ErrorKind::Other, "failed to get library output directory"))
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

    let libraries_path = match get_library_build_dir() {
        Ok(path) => path,
        Err(err) => panic!("{}", err)
    };

    println!("cargo:rustc-link-search=native={}", libraries_path.display());
    println!("cargo:rustc-link-lib=static=goto-programs");
    println!("cargo:rustc-link-lib=static=util");
    println!("cargo:rustc-link-lib=static=langapi");
    println!("cargo:rustc-link-lib=static=ansi-c");
    println!("cargo:rustc-link-lib=static=big-int");
    println!("cargo:rustc-link-lib=static=linking");
    println!("cargo:rustc-link-lib=static=goto-checker");
    println!("cargo:rustc-link-lib=static=cprover-api-cpp");
}
