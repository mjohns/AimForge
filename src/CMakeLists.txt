set(NAME "aim")

add_executable(${NAME} main.cc application.cc "util.h")

find_package(Vulkan REQUIRED)

target_link_libraries(${NAME}
  PUBLIC fmt SDL2::SDL2 SDL2::SDL2main imgui Vulkan::Vulkan)

target_include_directories(${NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
