#include "textures.h"

#include <SDL3_image/SDL_image.h>
#include <glad/glad.h>

#include <iostream>
#include <optional>

#include "aim/common/log.h"
#include "aim/graphics/image.h"

namespace aim {
namespace {
std::optional<GLenum> GetGlFormat(SDL_Surface* surface) {
  if (surface->format == SDL_PIXELFORMAT_RGBA32) {
    return GL_RGBA;
  }
  if (surface->format == SDL_PIXELFORMAT_RGB24) {
    return GL_RGB;
  }
  Logger::get()->warn("Invalid image format type for texture");
  return {};
}

}  // namespace

Texture::Texture(const std::filesystem::path& path) {
  glGenTextures(1, &texture_);
  glBindTexture(GL_TEXTURE_2D, texture_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  Image image(path);
  if (image.is_loaded()) {
    auto format = GetGlFormat(image.surface());
    if (format.has_value()) {
      is_loaded_ = true;
      height_ = image.height();
      width_ = image.width();
      glTexImage2D(GL_TEXTURE_2D,
                   0,
                   *format,
                   image.width(),
                   image.height(),
                   0,
                   *format,
                   GL_UNSIGNED_BYTE,
                   image.surface()->pixels);
      glGenerateMipmap(GL_TEXTURE_2D);
    }
  }
  if (!is_loaded_) {
    Logger::get()->error("Failed to load texture: {}", path.string());
  }
}

Texture::~Texture() {
  glDeleteTextures(1, &texture_);
}

TextureManager::TextureManager(std::vector<std::filesystem::path> texture_folders)
    : texture_folders_(std::move(texture_folders)) {}

Texture* TextureManager::GetTexture(const std::string& name) {
  auto it = texture_cache_.find(name);
  if (it != texture_cache_.end()) {
    return it->second.get();
  }

  // Need to load the texture
  for (auto& folder : texture_folders_) {
    auto path = folder / name;
    if (!std::filesystem::exists(path)) {
      continue;
    }
    auto texture = std::make_unique<Texture>(path);
    if (texture->is_loaded()) {
      Texture* result = texture.get();
      texture_cache_[name] = std::move(texture);
      return result;
    }
  }
  return nullptr;
}

}  // namespace aim
