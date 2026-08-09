#include "image.h"

#include "absl/cleanup/cleanup.h"
#include "aim/common/log.h"
#include "stripped_sdl_image.h"

namespace aim {

SDL_Surface* LoadImageSurface(const std::filesystem::path& path) {
  SDL_IOStream* src = SDL_IOFromFile(path.string().c_str(), "rb");
  if (!src) {
    return nullptr;
  }
  auto cleanup_src = absl::MakeCleanup([=]() { SDL_CloseIO(src); });

  /* See whether or not this data source can handle seeking */
  if (SDL_SeekIO(src, 0, SDL_IO_SEEK_CUR) < 0) {
    SDL_SetError("Can't seek in this data source");
    return nullptr;
  }

  if (IMG_isJPG(src) || IMG_isPNG(src)) {
    return IMG_LoadSTB_IO(src);
  }

  SDL_SetError("Unsupported image format");
  return nullptr;
}

Image::Image(const std::filesystem::path& path) {
  surface_ = LoadImageSurface(path);
  if (surface_ == nullptr) {
    Logger::get()->warn(
        "Failed to load image {}, IMG_GetError(): {}", path.string(), SDL_GetError());
    return;
  }
  width_ = surface_->w;
  height_ = surface_->h;
}

}  // namespace aim
