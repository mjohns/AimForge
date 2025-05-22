set(NAME "AimForge")

add_subdirectory(aim)

add_executable(
    ${NAME}
    main.cc )

if(WIN32)
    set_target_properties(${NAME}
    PROPERTIES
        LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE"
        LINK_FLAGS_RELEASE "/SUBSYSTEM:windows /ENTRY:mainCRTStartup"
        LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:windows /ENTRY:mainCRTStartup"
        LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:windows /ENTRY:mainCRTStartup"
    )
    target_link_libraries(${NAME} PRIVATE shell32)
endif(WIN32)


target_link_libraries(${NAME} PUBLIC aim SDL3::SDL3 imgui glm::glm-header-only)
target_include_directories(${NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


set (OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources")

file(COPY resources/scenarios DESTINATION "${OUTPUT_DIR}")
file(COPY resources/playlists DESTINATION "${OUTPUT_DIR}")
file(COPY resources/themes DESTINATION "${OUTPUT_DIR}")
file(COPY resources/fonts DESTINATION "${OUTPUT_DIR}")
file(COPY resources/textures DESTINATION "${OUTPUT_DIR}")
file(COPY resources/images DESTINATION "${OUTPUT_DIR}")
file(COPY resources/sounds DESTINATION "${OUTPUT_DIR}")

file(COPY aim/graphics/shaders/compiled DESTINATION "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders")
