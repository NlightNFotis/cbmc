# This step builds a binary driving the API (to be used for testing)
add_executable(api-binary-driver call_bmc.cpp api.cpp)
target_link_libraries(api-binary-driver goto-programs util langapi ansi-c)

# This step builds the API in the form of a statically linked library (libcprover-api.a)
add_library(cprover-api api.cpp)
target_link_libraries(cprover-api goto-programs util langapi ansi-c)

install(TARGETS cprover-api RUNTIME DESTINATION lib)
