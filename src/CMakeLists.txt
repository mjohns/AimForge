set(NAME "aim")

add_executable(
    ${NAME}
    main.cc
    application.cc
    model.cc
    camera.cc
    time_util.cc
    sound.cc)

find_package(OpenGL REQUIRED)

target_link_libraries(${NAME}
  PUBLIC fmt
         SDL3::SDL3
         SDL3_mixer::SDL3_mixer
         imgui
         ${OPENGL_LIBRARIES}
         FlatBuffers)

target_include_directories(${NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${glm_SOURCE_DIR})
