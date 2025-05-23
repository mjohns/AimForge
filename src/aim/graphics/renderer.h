#pragma once

#include <SDL3/SDL.h>

#include <filesystem>
#include <memory>
#include <vector>

#include "aim/common/times.h"
#include "aim/core/perf.h"
#include "aim/core/target.h"
#include "aim/graphics/textures.h"
#include "aim/proto/scenario.pb.h"
#include "aim/proto/theme.pb.h"
#include "aim/proto/settings.pb.h"

namespace aim {

struct RenderContext {
  SDL_GPUCommandBuffer* command_buffer = nullptr;
  SDL_GPUTexture* swapchain_texture = nullptr;
  SDL_GPURenderPass* render_pass = nullptr;
};

class Renderer {
 public:
  Renderer() {}
  virtual ~Renderer() {}

  virtual void DrawScenario(const glm::mat4& projection,
                            const Room& room,
                            const Theme& theme,
                            const HealthBarSettings& health_bar,
                            const std::vector<Target>& targets,
                            const glm::mat4& view,
                            RenderContext* ctx,
                            const Stopwatch& stopwatch,
                            FrameTimes* times) = 0;

  virtual void Cleanup() = 0;
};

std::unique_ptr<Renderer> CreateRenderer(const std::vector<std::filesystem::path>& texture_dirs,
                                         const std::filesystem::path& shader_dir,
                                         SDL_GPUDevice* device,
                                         SDL_Window* sdl_window);

}  // namespace aim
