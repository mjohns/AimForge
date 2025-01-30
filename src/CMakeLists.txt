set(NAME "aim")

add_executable(
    ${NAME}
    main.cc
    application.cc
    model.cc
    camera.cc
    time_util.cc
    shader.cc
    sound.cc
    room.cc
    glad_loader.cc
    sphere.cc)

find_package(OpenGL REQUIRED)

target_link_libraries(${NAME}
  PUBLIC fmt
         SDL3::SDL3
         SDL3_mixer::SDL3_mixer
         imgui
         glad
         ${OPENGL_LIBRARIES}
         FlatBuffers)

target_include_directories(${NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${glm_SOURCE_DIR})
