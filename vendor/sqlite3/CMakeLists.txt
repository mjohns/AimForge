add_library(sqlite3 STATIC sqlite3.c)

target_include_directories(sqlite3 PUBLIC .)
