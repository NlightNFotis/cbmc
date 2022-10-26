# This step builds a binary driving the API (to be used for testing)
add_executable(api_bin call_bmc.cpp api.cpp)
target_link_libraries(api_bin goto-programs util langapi ansi-c)

# This step builds the API in the form of a statically linked library (libbmc_api.a)
add_library(bmc_api api.cpp)
target_link_libraries(bmc_api goto-programs util langapi ansi-c)

install(TARGETS bmc_api RUNTIME DESTINATION lib)
