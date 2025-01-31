set(NAME "aim")

add_library(
    ${NAME}
    STATIC
    core/application.cc
    core/camera.cc
    scenario/static_scenario.cc
    scenario/target.cc
    scenario/scenario_timer.cc
    common/time_util.cc
    audio/sound.cc
    graphics/shader.cc
    graphics/room.cc
    graphics/crosshair.cc
    graphics/glad_loader.cc
    graphics/sphere.cc)

find_package(OpenGL REQUIRED)

target_link_libraries(${NAME}
  PUBLIC fmt
         SDL3::SDL3
         SDL3_mixer::SDL3_mixer
         imgui
         glad
         ${OPENGL_LIBRARIES}
         FlatBuffers)

target_include_directories(${NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/.. ${glm_SOURCE_DIR})
