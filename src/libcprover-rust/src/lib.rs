#[cxx::bridge]
pub mod ffi {

    unsafe extern "C++" {
        include!("api.h");
        include!("include/c_api.h");

        type api_sessiont;

        // Functions from api.h
        fn new_api_session() -> UniquePtr<api_sessiont>;
        fn get_api_version(api_handle: UniquePtr<api_sessiont>) -> UniquePtr<CxxString>;
        fn load_model_from_files(api_handle: UniquePtr<api_sessiont>, files: &CxxVector<CxxString>);

        // Functions from c_api.h
        fn translate_vector_of_string(elements: Vec<String>) -> &'static CxxVector<CxxString>;
    }
}

// To test run "cargo test -- --nocapture"
#[cfg(test)]
mod tests {
    use super::*;
    use cxx::let_cxx_string;
    use cxx::{CxxString, CxxVector};
    use core::pin::Pin;

    #[test]
    fn it_works() {
        let client = ffi::new_api_session();
        let result = ffi::get_api_version(client);

        let_cxx_string!(expected_version = "0.1");
        assert_eq!(*result, *expected_version);
    }

    #[test]
    fn translate_vector_of_rust_string_to_CPP() {
        let vec: Vec<String> = vec![
            "/tmp/example.c".to_owned(),
            "/tmp/example2.c".to_owned()];

        let vect = ffi::translate_vector_of_string(vec);
        assert_eq!(vect.len(), 2);
    }
    
    #[test]
    fn it_can_load_model_from_file() {
        let client = ffi::new_api_session();
    
        let vec: Vec<String> = vec![
            "/tmp/example.c".to_owned()];

        let vect = ffi::translate_vector_of_string(vec);
        assert_eq!(vect.len(), 1);

        // Invoke load_model_from_files and see if the model
        // has been loaded.
        ffi::load_model_from_files(client, vect);
    }

}
