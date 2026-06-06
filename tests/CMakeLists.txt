# Nessy test suite — Phase A.1+
CPMAddPackage("gh:catchorg/Catch2@3.5.2")
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)

add_executable(nessy_tests
    cpu/test_km6502_functional.cpp
)
target_compile_features(nessy_tests PRIVATE cxx_std_20)
target_link_libraries(nessy_tests PRIVATE Catch2::Catch2WithMain)

# Run tests from the executable's own dir so relative paths (e.g. roms/) resolve.
catch_discover_tests(nessy_tests WORKING_DIRECTORY $<TARGET_FILE_DIR:nessy_tests>)
