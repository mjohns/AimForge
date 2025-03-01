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
endif(WIN32)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_link_libraries(${NAME} PUBLIC aim)
target_include_directories(${NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


set (OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources")

file(GLOB SOUND_FILES resources/sounds/*)
file(MAKE_DIRECTORY "${OUTPUT_DIR}/sounds")
foreach(THE_FILE ${SOUND_FILES})
    file(COPY
        "${THE_FILE}"
        DESTINATION "${OUTPUT_DIR}/sounds"
    )
endforeach()

file(COPY resources/scenarios DESTINATION "${OUTPUT_DIR}")
file(COPY resources/playlists DESTINATION "${OUTPUT_DIR}")
file(COPY resources/themes DESTINATION "${OUTPUT_DIR}")
file(COPY resources/fonts DESTINATION "${OUTPUT_DIR}")
file(COPY resources/textures DESTINATION "${OUTPUT_DIR}")
file(COPY resources/images DESTINATION "${OUTPUT_DIR}")
