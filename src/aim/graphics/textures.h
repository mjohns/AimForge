#pragma once

#include <SDL3/SDL.h>
#include <imgui.h>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace aim {

class Texture {
 public:
  explicit Texture(const std::filesystem::path& path, SDL_GPUDevice* device);
  ~Texture();

  SDL_GPUTexture* texture() {
    return texture_;
  }

  SDL_GPUSampler* sampler() {
    return sampler_;
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

  ImTextureID GetImTextureId() {
    return (ImTextureID)&texture_sampler_binding_;
  }

  Texture(const Texture&) = delete;
  Texture(Texture&&) = default;
  Texture& operator=(Texture other) = delete;
  Texture& operator=(Texture&& other) = delete;

 private:
  SDL_GPUTexture* texture_ = nullptr;
  SDL_GPUSampler* sampler_ = nullptr;
  SDL_GPUTextureSamplerBinding texture_sampler_binding_{};

  bool is_loaded_ = false;
  int height_ = 0;
  int width_ = 0;
};

class TextureManager {
 public:
  explicit TextureManager(std::vector<std::filesystem::path> texture_folders,
                          SDL_GPUDevice* device);

  Texture* GetTexture(const std::string& name);

 private:
  std::unordered_map<std::string, std::unique_ptr<Texture>> texture_cache_;
  std::vector<std::filesystem::path> texture_folders_;
  SDL_GPUDevice* gpu_device_;
};

}  // namespace aim
