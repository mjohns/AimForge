#include "textures.h"

#include <SDL3_image/SDL_image.h>
#include <glad/glad.h>

#include <iostream>
#include <optional>

#include "aim/common/log.h"
#include "aim/graphics/image.h"

namespace aim {

Texture::Texture(const std::filesystem::path& path, SDL_GPUDevice* device) : gpu_device_(device) {
  Image image(path);
  if (!image.is_loaded()) {
    Logger::get()->error("Failed to load texture image: {}", path.string());
    return;
  }

  SDL_Surface* image_surface = SDL_ConvertSurface(image.surface(), SDL_PIXELFORMAT_ABGR8888);
  if (image_surface == nullptr || image_surface->format != SDL_PIXELFORMAT_ABGR8888) {
    SDL_DestroySurface(image_surface);
    return;
  }

  SDL_GPUSamplerCreateInfo sampler_create_info{};
  sampler_create_info.min_filter = SDL_GPU_FILTER_LINEAR;
  sampler_create_info.mag_filter = SDL_GPU_FILTER_LINEAR;
  sampler_create_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
  sampler_create_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  sampler_create_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  sampler_create_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
  sampler_create_info.enable_anisotropy = true;
  sampler_create_info.max_anisotropy = 4;
  sampler_ = SDL_CreateGPUSampler(device, &sampler_create_info);

  SDL_GPUTextureCreateInfo create_info{};
  create_info.type = SDL_GPU_TEXTURETYPE_2D;
  create_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  create_info.width = image.width();
  create_info.height = image.height();
  create_info.layer_count_or_depth = 1;
  create_info.num_levels = 4;
  create_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
  texture_ = SDL_CreateGPUTexture(device, &create_info);

  SDL_GPUTransferBufferCreateInfo transfer_create_info{};
  transfer_create_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  int size = image.width() * image.height() * 4;
  transfer_create_info.size = size;

  SDL_GPUTransferBuffer* transfer_buffer =
      SDL_CreateGPUTransferBuffer(device, &transfer_create_info);

  Uint8* texture_transfer_ptr = (Uint8*)SDL_MapGPUTransferBuffer(device, transfer_buffer, true);
  SDL_memcpy(texture_transfer_ptr, image_surface->pixels, size);
  SDL_UnmapGPUTransferBuffer(device, transfer_buffer);

  // Upload the transfer data to the GPU resources
  SDL_GPUCommandBuffer* upload_cmd_buffer = SDL_AcquireGPUCommandBuffer(device);

  SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(upload_cmd_buffer);
  SDL_GPUTextureTransferInfo texture_transfer_info{};
  texture_transfer_info.transfer_buffer = transfer_buffer;
  texture_transfer_info.offset = 0;

  SDL_GPUTextureRegion region{};
  region.texture = texture_;
  region.w = image.width();
  region.h = image.height();
  region.d = 1;
  SDL_UploadToGPUTexture(copy_pass, &texture_transfer_info, &region, true);

  SDL_EndGPUCopyPass(copy_pass);
  SDL_GenerateMipmapsForGPUTexture(upload_cmd_buffer, texture_);
  SDL_SubmitGPUCommandBuffer(upload_cmd_buffer);

  SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
  SDL_DestroySurface(image_surface);

  texture_sampler_binding_.texture = texture_;
  texture_sampler_binding_.sampler = sampler_;
  width_ = image.width();
  height_ = image.height();
  is_loaded_ = true;
}

Texture::~Texture() {
  if (texture_ != nullptr) {
    SDL_ReleaseGPUTexture(gpu_device_, texture_);
  }
  if (sampler_ != nullptr) {
    SDL_ReleaseGPUSampler(gpu_device_, sampler_);
  }
}

TextureManager::TextureManager(std::vector<std::filesystem::path> texture_folders,
                               SDL_GPUDevice* device)
    : texture_folders_(std::move(texture_folders)), gpu_device_(device) {}

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
    auto texture = std::make_unique<Texture>(path, gpu_device_);
    if (texture->is_loaded()) {
      Texture* result = texture.get();
      texture_cache_[name] = std::move(texture);
      return result;
    }
  }
  return nullptr;
}

}  // namespace aim
