#include "textures.h"

#include <glad/glad.h>

#include <iostream>

#include "aim/common/log.h"
#include "aim/graphics/image.h"

namespace aim {

Texture::Texture(const std::filesystem::path& path) {
  glGenTextures(1, &texture_);
  glBindTexture(GL_TEXTURE_2D, texture_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  // set texture filtering parameters
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // load image, create texture and generate mipmaps
  Image image(path, /* flip_vertically= */ true);
  if (image.is_loaded()) {
    is_loaded_ = true;
    height_ = image.height();
    width_ = image.width();
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGB,
                 image.width(),
                 image.height(),
                 0,
                 GL_RGB,
                 GL_UNSIGNED_BYTE,
                 image.data());
    glGenerateMipmap(GL_TEXTURE_2D);
  } else {
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
