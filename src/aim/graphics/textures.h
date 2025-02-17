#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace aim {

class Texture {
 public:
  explicit Texture(const std::filesystem::path& path);
  ~Texture();

  unsigned int id() {
    return texture_;
  }

  bool is_loaded() {
    return is_loaded_;
  }

  int height() {
    return height_;
  }

  int width() {
    return width_;
  }

  Texture(const Texture&) = delete;
  Texture(Texture&&) = default;
  Texture& operator=(Texture other) = delete;
  Texture& operator=(Texture&& other) = delete;

 private:
  unsigned int texture_;
  bool is_loaded_ = false;
  int height_ = 0;
  int width_ = 0;
};

class TextureManager {
 public:
  explicit TextureManager(std::vector<std::filesystem::path> texture_folders);

  Texture* GetTexture(const std::string& name);

 private:
  std::unordered_map<std::string, std::unique_ptr<Texture>> texture_cache_;
  std::vector<std::filesystem::path> texture_folders_;
};

}  // namespace aim
