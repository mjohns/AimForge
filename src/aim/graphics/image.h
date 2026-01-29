#pragma once

#include <filesystem>

#include "SDL3/SDL.h"

namespace aim {

SDL_Surface* LoadImageSurface(const std::filesystem::path& path);

class Image {
 public:
  explicit Image(const std::filesystem::path& path);

  ~Image() {
    if (surface_ != nullptr) {
      SDL_DestroySurface(surface_);
    }
  }

  bool is_loaded() {
    return surface_ != nullptr;
  }

  int width() {
    return width_;
  }

  int height() {
    return height_;
  }

  SDL_Surface* surface() {
    return surface_;
  }

  Image(const Image&) = delete;
  Image(Image&&) = default;
  Image& operator=(Image other) = delete;
  Image& operator=(Image&& other) = delete;

 private:
  SDL_Surface* surface_ = nullptr;
  int width_ = 0;
  int height_ = 0;
};

}  // namespace aim