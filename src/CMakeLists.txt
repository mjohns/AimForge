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

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

target_link_libraries(${NAME} PUBLIC aim)
target_include_directories(${NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set(SOUND_FILES
  "kill_confirmed.ogg"
  "shoot.ogg"
  "bass_drum.ogg"
  "short_bass.wav"
  "body_shot.ogg"
)

set (SOUND_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/sounds")
file(MAKE_DIRECTORY "${SOUND_OUTPUT_DIR}")
foreach(SOUND_FILE ${SOUND_FILES})
    file(COPY
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sounds/${SOUND_FILE}"
        DESTINATION "${SOUND_OUTPUT_DIR}"
    )
endforeach()

set(FONT_FILES
  "Manrope.ttf"
)

set (FONTS_OUTPUT_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/fonts")
file(MAKE_DIRECTORY "${FONTS_OUTPUT_DIR}")
foreach(FONT_FILE ${FONT_FILES})
    file(COPY
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/${FONT_FILE}"
        DESTINATION "${FONTS_OUTPUT_DIR}"
    )
endforeach()
