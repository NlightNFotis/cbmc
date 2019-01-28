set (CMAKE_CXX_STANDARD 11)

file(GLOB_RECURSE sources "*.cpp" "*.h")
add_executable(goto-harness ${sources})
generic_includes(goto-harness)

target_link_libraries(goto-harness
    goto-programs
    util
)
