add_library(implot implot.cpp implot_items.cpp)

target_link_libraries(implot PUBLIC imgui)
target_include_directories(implot PUBLIC .)
