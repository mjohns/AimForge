enable_testing()

set(NAME AimForgeTests) 

file(GLOB_RECURSE TEST_SOURCES *test.cc)

# Add the test executable
add_executable(${NAME} ${TEST_SOURCES})

# Link your test executable to your library and GTest
target_link_libraries(
    ${NAME}
    GTest::gtest_main
    aim
)

# Discover tests using gtest_discover_tests
# This will automatically find all TEST() and TEST_F() macros
# and register them with CTest.
include(GoogleTest) # This module provides gtest_discover_tests

gtest_add_tests(${NAME} "" ${TEST_SOURCES})