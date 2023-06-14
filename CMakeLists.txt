cmake_minimum_required(VERSION 3.11)
project(LookUB)

set(CMAKE_CXX_STANDARD 17)

enable_testing()
set(TARGET_RUNTIME_DIR "${CMAKE_BINARY_DIR}/runtime")
add_subdirectory(scc)
include(GoogleTest)

set(FUZZ_PROJECT_NAME LookUB)
add_subdirectory(mutator)
add_subdirectory(main)
