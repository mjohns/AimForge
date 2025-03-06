add_library(imgui STATIC
  imconfig.h
  imgui.cpp
  imgui.h
  imgui_draw.cpp
  imgui_internal.h
  imgui_tables.cpp
  imgui_widgets.cpp
  imstb_rectpack.h
  imstb_textedit.h
  imstb_truetype.h
  imgui_demo.cpp
  misc/cpp/imgui_stdlib.h
  misc/cpp/imgui_stdlib.cpp
  backends/imgui_impl_sdl3.h
  backends/imgui_impl_sdl3.cpp
  backends/imgui_impl_sdl3.cpp
  backends/imgui_impl_sdlgpu3_shaders.h
  backends/imgui_impl_sdlgpu3.h
  backends/imgui_impl_sdlgpu3.cpp
  backends/imgui_impl_opengl3.h
  backends/imgui_impl_opengl3.cpp)

target_include_directories(imgui PUBLIC .)
target_compile_features(imgui PRIVATE cxx_std_20)

target_link_libraries(imgui PUBLIC SDL3::SDL3)
