add_library(SDL_image
  stripped_sdl_image.h
  stripped_sdl_image.cc
  internal_sdl_stb_image.h)

target_include_directories(SDL_image PUBLIC .)
target_link_libraries(SDL_image PUBLIC SDL3::SDL3)
