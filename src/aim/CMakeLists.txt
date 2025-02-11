set(NAME "aim")

file(GLOB_RECURSE SRCS *.cc)
file(GLOB_RECURSE HDRS *.h)

add_library(${NAME} STATIC ${SRCS} ${HRDS})

find_package(OpenGL REQUIRED)


target_link_libraries(${NAME}
  PUBLIC SDL3::SDL3
         SDL3_mixer::SDL3_mixer
         imgui
         implot
         glad
         sqlite3
         protobuf::libprotobuf
         ${OPENGL_LIBRARIES})

target_include_directories(${NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/.. ${glm_SOURCE_DIR})
