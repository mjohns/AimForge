set(NAME AimForgeTests) 

file(GLOB_RECURSE TEST_SOURCES *.cc)

# Add the test executable
add_executable(${NAME} ${TEST_SOURCES})

# Link your test executable to your library and GTest
target_link_libraries(
    ${NAME}
    GTest::gmock_main
    GTest::gmock
    protobuf-matchers
    aim
)

if(MSVC)
    # Disables warnings from protobuf lib
    target_compile_options(${NAME} PRIVATE /wd4091)
endif()