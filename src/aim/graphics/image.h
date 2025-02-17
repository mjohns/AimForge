#pragma once

#include <stb_image.h>

namespace aim {

class Image {
 public:
  explicit Image(const std::filesystem::path& path, bool flip_vertically = false) {
    int nr_channels;
    stbi_set_flip_vertically_on_load(flip_vertically);
    data_ = stbi_load(path.string().c_str(), &width_, &height_, &nr_channels, 0);
  }

  ~Image() {
    if (data_ != nullptr) {
      stbi_image_free(data_);
    }
  }

  bool is_loaded() {
    return data_ != nullptr;
  }

  int width() {
    return width_;
  }

  int height() {
    return height_;
  }

  unsigned char* data() {
    return data_;
  }

  Image(const Image&) = delete;
  Image(Image&&) = default;
  Image& operator=(Image other) = delete;
  Image& operator=(Image&& other) = delete;

 private:
  unsigned char* data_ = nullptr;
  int width_ = 0;
  int height_ = 0;
};

}  // namespace aim