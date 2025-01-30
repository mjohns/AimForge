set(NAME "aim_trainer")

add_subdirectory(aim)

add_executable(
    ${NAME}
    main.cc )

target_link_libraries(${NAME} PUBLIC aim)
target_include_directories(${NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
