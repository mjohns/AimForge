set(NAME "aim")

file(GLOB_RECURSE SRCS *.cc)
file(GLOB_RECURSE HDRS *.h)

add_library(${NAME} STATIC ${SRCS} ${HRDS} "database/history_db.h")

find_package(OpenGL REQUIRED)


target_link_libraries(${NAME}
  PUBLIC SDL3::SDL3
         SDL3_mixer::SDL3_mixer
         SDL3_image::SDL3_image
         stb
         json
         glm::glm-header-only
         imgui
         implot
         glad
         sqlite3
         protobuf::libprotobuf
         spdlog
         ${OPENGL_LIBRARIES})

target_include_directories(${NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
