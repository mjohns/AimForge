add_library(imgui STATIC
  imgui/imconfig.h
  imgui/imgui.cpp
  imgui/imgui.h
  imgui/imgui_draw.cpp
  imgui/imgui_internal.h
  imgui/imgui_tables.cpp
  imgui/imgui_widgets.cpp
  imgui/imstb_rectpack.h
  imgui/imstb_textedit.h
  imgui/imstb_truetype.h
  imgui/imgui_demo.cpp
  imgui/misc/cpp/imgui_stdlib.h
  imgui/misc/cpp/imgui_stdlib.cpp
  imgui/backends/imgui_impl_sdl3.h
  imgui/backends/imgui_impl_sdl3.cpp
  imgui/backends/imgui_impl_sdl3.cpp
  imgui/backends/imgui_impl_sdlgpu3_shaders.h
  imgui/backends/imgui_impl_sdlgpu3.h
  imgui/backends/imgui_impl_sdlgpu3.cpp)

target_include_directories(imgui PUBLIC .)
target_include_directories(imgui PUBLIC imgui)
target_compile_features(imgui PRIVATE cxx_std_20)

target_link_libraries(imgui PUBLIC SDL3::SDL3)
