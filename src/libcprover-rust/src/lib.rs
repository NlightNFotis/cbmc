#[cxx::bridge]
pub mod ffi {
    unsafe extern "C++" {
        include!("api.h");
        type api_sessiont;

        fn new_api_session() -> UniquePtr<api_sessiont>;
        fn get_api_version(api_handle: UniquePtr<api_sessiont>) -> UniquePtr<CxxString>;
    }
}

// To test run "cargo test -- --nocapture"
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn it_works() {
        let client = ffi::new_api_session();
        let result = ffi::get_api_version(client);
        println!("{}", *result);
        // assert_eq!(*result, "0.1");
    }
}
