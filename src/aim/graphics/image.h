#pragma once

#include <SDL3_image/SDL_image.h>

#include <filesystem>

#include "aim/common/log.h"

namespace aim {

class Image {
 public:
  explicit Image(const std::filesystem::path& path) {
    surface_ = IMG_Load(path.string().c_str());
    if (surface_ == nullptr) {
      Logger::get()->warn(
          "Failed to load image {}, IMG_GetError(): {}", path.string(), SDL_GetError());
      return;
    }
    width_ = surface_->w;
    height_ = surface_->h;
  }

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