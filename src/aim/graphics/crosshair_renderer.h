#pragma once

#include <filesystem>

#include "SDL3/SDL.h"  // IWYU pragma: keep
#include "aim/common/simple_types.h"
#include "aim/graphics/textures.h"
#include "aim/proto/crosshair.pb.h"
#include "aim/proto/theme.pb.h"
#include "imgui.h"

namespace aim {

class CrosshairRenderer {
 public:
  explicit CrosshairRenderer(const std::filesystem::path& crosshair_dir, SDL_GPUDevice* gpu_device);
  AIM_NO_COPY(CrosshairRenderer);

  void Draw(const Crosshair& crosshair,
            float crosshair_size,
            const Theme& theme,
            const ImVec2& center);

  void DrawLayer(const CrosshairLayer& layer,
                 float crosshair_size,
                 const Theme& theme,
                 const ImVec2& center);

 private:
  TextureManager texture_manager_;
};

}  // namespace aim
