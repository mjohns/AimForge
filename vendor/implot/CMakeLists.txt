add_library(implot STATIC implot.cpp implot_items.cpp)

target_link_libraries(implot PUBLIC imgui)
target_include_directories(implot PUBLIC .)
