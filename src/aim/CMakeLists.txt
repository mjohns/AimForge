set(NAME "aim")

file(GLOB_RECURSE SRCS *.cc)
file(GLOB_RECURSE HDRS *.h)

add_library(${NAME} STATIC ${SRCS} ${HRDS} )

target_link_libraries(${NAME}
  PUBLIC SDL3::SDL3
          SDL3_mixer::SDL3_mixer
          json
          glm::glm-header-only
          imgui
          implot
          sqlite3
          protobuf::libprotobuf
          spdlog)

target_include_directories(${NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

set_target_properties(${NAME} PROPERTIES COMPILE_WARNING_AS_ERROR ON)

if(MSVC)
    # Disables warnings from protobuf lib
    target_compile_options(${NAME} PRIVATE /wd4091)
endif()
