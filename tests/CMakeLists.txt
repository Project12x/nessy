# Nessy test suite — Phase A.1+
CPMAddPackage("gh:catchorg/Catch2@3.5.2")
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)

add_executable(nessy_tests
    cpu/Km6502Harness.cpp
    cpu/test_km6502_functional.cpp
)
target_compile_features(nessy_tests PRIVATE cxx_std_20)
target_include_directories(nessy_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu
    ${CMAKE_SOURCE_DIR}/src/apu/nsfplay/xgm/devices/CPU/km6502
)
target_link_libraries(nessy_tests PRIVATE Catch2::Catch2WithMain)

# Copy ROM fixtures next to the test executable so relative paths resolve.
add_custom_command(TARGET nessy_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/cpu/roms $<TARGET_FILE_DIR:nessy_tests>/roms)

# Run tests from the executable's own dir so relative paths (e.g. roms/) resolve.
catch_discover_tests(nessy_tests WORKING_DIRECTORY $<TARGET_FILE_DIR:nessy_tests>)
