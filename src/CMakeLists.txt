set(NAME "aim_trainer")

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

target_link_libraries(${NAME} PUBLIC aim)
target_include_directories(${NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set(SOUND_FILES
  "kill_confirmed.ogg"
  "shoot.ogg"
  "bass_drum.ogg"
  "short_bass.wav"
)

set (SOUND_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/sounds")
file(MAKE_DIRECTORY "${SOUND_OUTPUT_DIR}")
foreach(SOUND_FILE ${SOUND_FILES})
    file(COPY
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sounds/${CONFIG_FILE}"
        DESTINATION "${SOUND_OUTPUT_DIR}"
    )
endforeach()
