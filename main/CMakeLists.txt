add_executable(${FUZZ_PROJECT_NAME} main.cpp )
target_link_libraries(${FUZZ_PROJECT_NAME} PUBLIC
  scc-driver
  LookUB-mutator
)

add_dependencies(${FUZZ_PROJECT_NAME} scc-driver)
set_target_properties(${FUZZ_PROJECT_NAME}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
