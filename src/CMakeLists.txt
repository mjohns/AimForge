set(NAME "aim")

add_executable(${NAME} main.cc application.cc model.cc camera.cc)

find_package(Vulkan REQUIRED)

target_link_libraries(${NAME}
  PUBLIC fmt SDL3::SDL3 imgui Vulkan::Vulkan)

target_include_directories(${NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
