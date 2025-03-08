#include "renderer.h"

#include "aim/common/log.h"
#include "aim/common/util.h"

namespace aim {
namespace {

SDL_GPUShader* LoadShader(SDL_GPUDevice* device,
                          const std::filesystem::path& shader_path,
                          SDL_GPUShaderStage stage,
                          Uint32 sampler_count = 0,
                          Uint32 uniform_buffer_count = 0,
                          Uint32 storage_buffer_count = 0,
                          Uint32 storage_texture_count = 0) {
  const char* entrypoint;
  SDL_GPUShaderFormat backend_formats = SDL_GetGPUShaderFormats(device);
  SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;
  if (backend_formats & SDL_GPU_SHADERFORMAT_SPIRV) {
    format = SDL_GPU_SHADERFORMAT_SPIRV;
    entrypoint = "main";
  }

  std::string filename = shader_path.string();

  size_t code_size;
  void* code = SDL_LoadFile(filename.c_str(), &code_size);
  if (code == nullptr) {
    Logger::get()->error("Unable to load shader {}, SDL_GetError(): {}", filename, SDL_GetError());
    return nullptr;
  }

  SDL_GPUShaderCreateInfo shader_info;
  shader_info.code_size = code_size;
  shader_info.code = (Uint8*)code;
  shader_info.entrypoint = entrypoint;
  shader_info.format = format;
  shader_info.stage = stage;
  shader_info.num_samplers = sampler_count;
  shader_info.num_uniform_buffers = uniform_buffer_count;
  shader_info.num_storage_textures = storage_texture_count;
  shader_info.num_storage_buffers = storage_buffer_count;

  SDL_GPUShader* shader = SDL_CreateGPUShader(device, &shader_info);

  if (shader == nullptr) {
    Logger::get()->error("ERROR: SDL_CreateGPUShader failed: {}", SDL_GetError());
    SDL_free(code);
    return nullptr;
  }
  SDL_free(code);
  return shader;
}

class RendererImpl : public Renderer {
 public:
  RendererImpl(const std::vector<std::filesystem::path>& texture_dirs)
      : texture_manager_(texture_dirs) {}

  bool Initialize(const std::filesystem::path& shader_dir, SDL_GPUDevice* device) {
    solid_color_vertex_shader_ = LoadShader(
        device, (shader_dir / "solid_color.vert.spv").string().c_str(), SDL_GPU_SHADERSTAGE_VERTEX);
    solid_color_vertex_shader_ = LoadShader(device,
                                            (shader_dir / "solid_color.frag.spv").string().c_str(),
                                            SDL_GPU_SHADERSTAGE_FRAGMENT);
    return true;
  }

  void DrawScenario(const glm::mat4& projection,
                    const Room& room,
                    const Theme& theme,
                    const std::vector<Target>& targets,
                    const glm::mat4& view,
                    RenderContext* ctx,
                    const Stopwatch& stopwatch,
                    FrameTimes* times) override {}

 private:
  SDL_GPUShader* solid_color_fragment_shader_ = nullptr;
  SDL_GPUShader* solid_color_vertex_shader_ = nullptr;
  TextureManager texture_manager_;
};

}  // namespace

/*
void Renderer::DrawScenario(const Room& room,
                            const Theme& theme,
                            const std::vector<Target>& targets,
                            const glm::mat4& view,
                            const Stopwatch& stopwatch,
                            FrameTimes* times) {
  times->render_room_start = stopwatch.GetElapsedMicros();
  room_renderer_.Draw(room, theme, view);
  times->render_room_end = stopwatch.GetElapsedMicros();
  glm::vec3 target_color = theme.has_target_color() ? ToVec3(theme.target_color()) : glm::vec3(0);
  glm::vec3 ghost_target_color =
      theme.has_ghost_target_color() ? ToVec3(theme.ghost_target_color()) : glm::vec3(0.3);

  times->render_targets_start = stopwatch.GetElapsedMicros();
  std::vector<Sphere> target_spheres;
  std::vector<Sphere> ghost_spheres;
  for (const Target& target : targets) {
    if (target.ShouldDraw()) {
      std::vector<Sphere>& spheres = target.is_ghost ? ghost_spheres : target_spheres;
      glm::vec3 color = target.is_ghost ? ghost_target_color : target_color;
      if (target.is_pill) {
        Cylinder c;
        c.radius = target.radius;
        c.up = target.pill_up;
        c.height = target.height - target.radius;
        c.position = target.position;
        cylinder_renderer_.Draw(ToVec3(room.camera_position()), view, color, {c});

        spheres.push_back({});
        Sphere& top = spheres.back();
        top.position = c.position + c.up * (c.height * 0.5f);
        top.radius = target.radius;

        spheres.push_back({});
        Sphere& bottom = spheres.back();
        bottom.position = c.position + c.up * (c.height * -0.5f);
        bottom.radius = target.radius;
      } else {
        spheres.push_back({});
        Sphere& s = spheres.back();
        s.position = target.position;
        s.radius = target.radius;
      }
    }
  }
  sphere_renderer_.Draw(view, target_color, target_spheres);
  sphere_renderer_.Draw(view, ghost_target_color, ghost_spheres);
  times->render_targets_end = stopwatch.GetElapsedMicros();
}
*/

std::unique_ptr<Renderer> CreateRenderer(const std::vector<std::filesystem::path>& texture_dirs,
                                         const std::filesystem::path& shader_dir,
                                         SDL_GPUDevice* device) {
  auto renderer = std::make_unique<RendererImpl>(texture_dirs);
  if (!renderer->Initialize(shader_dir, device)) {
    return {};
  }
  return std::move(renderer);
}

}  // namespace aim
