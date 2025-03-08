#include "renderer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

#include "aim/common/log.h"
#include "aim/common/util.h"
#include "aim/graphics/shapes.h"

namespace aim {
namespace {

SDL_GPUShader* LoadShader(SDL_GPUDevice* device,
                          const std::filesystem::path& shader_path,
                          SDL_GPUShaderStage stage,
                          Uint32 uniform_buffer_count = 0,
                          Uint32 sampler_count = 0,
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

SDL_GPUColorTargetBlendState DefaultBlendState() {
  SDL_GPUColorTargetBlendState blend_state{};
  blend_state.enable_blend = true;
  blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
  blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
  blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
  blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
  blend_state.color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G |
                                 SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;
  return blend_state;
}

class RendererImpl : public Renderer {
 public:
  RendererImpl(const std::vector<std::filesystem::path>& texture_dirs,
               SDL_GPUDevice* device,
               SDL_Window* sdl_window)
      : texture_manager_(texture_dirs), device_(device), sdl_window_(sdl_window) {}

  ~RendererImpl() override {
    Cleanup();
  }

  void Cleanup() override {
    CleanupShaders();
    if (sphere_pipeline_ != nullptr) {
      SDL_ReleaseGPUGraphicsPipeline(device_, sphere_pipeline_);
      sphere_pipeline_ = nullptr;
    }
  }

  void CleanupShaders() {
    if (solid_color_vertex_shader_ != nullptr) {
      SDL_ReleaseGPUShader(device_, solid_color_vertex_shader_);
      solid_color_vertex_shader_ = nullptr;
    }
    if (solid_color_fragment_shader_ != nullptr) {
      SDL_ReleaseGPUShader(device_, solid_color_fragment_shader_);
      solid_color_fragment_shader_ = nullptr;
    }
  }

  bool Initialize(const std::filesystem::path& shader_dir) {
    solid_color_vertex_shader_ = LoadShader(device_,
                                            (shader_dir / "solid_color.vert.spv").string().c_str(),
                                            SDL_GPU_SHADERSTAGE_VERTEX,
                                            1);
    solid_color_fragment_shader_ =
        LoadShader(device_,
                   (shader_dir / "solid_color.frag.spv").string().c_str(),
                   SDL_GPU_SHADERSTAGE_FRAGMENT,
                   1);

    if (!CreateSpherePipeline()) {
      return false;
    }

    SDL_GPUCommandBuffer* upload_command_buffer = SDL_AcquireGPUCommandBuffer(device_);
    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(upload_command_buffer);

    SDL_GPUTransferBuffer* sphere_transfer_buffer = CreateSphereVertexBuffer(copy_pass);

    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(upload_command_buffer);
    SDL_ReleaseGPUTransferBuffer(device_, sphere_transfer_buffer);

    CleanupShaders();
    return true;
  }

  void DrawScenario(const glm::mat4& projection,
                    const Room& room,
                    const Theme& theme,
                    const std::vector<Target>& targets,
                    const glm::mat4& view,
                    RenderContext* ctx,
                    const Stopwatch& stopwatch,
                    FrameTimes* times) override {
    const glm::mat4 view_projection = projection * view;

    /*
    SDL_BindGPUGraphicsPipeline(ctx->render_pass, sphere_pipeline_);
    SDL_GPUBufferBinding binding{};
    binding.buffer = sphere_vertex_buffer_;
    binding.offset = 0;
    SDL_BindGPUVertexBuffers(ctx->render_pass, 0, &binding, 1);
    glm::mat4 transform(1.0f);
    SDL_PushGPUVertexUniformData(ctx->command_buffer, 0, &transform[0][0], sizeof(glm::mat4));

    glm::vec4 color4(1, 1, 0, 1);
    SDL_PushGPUFragmentUniformData(ctx->command_buffer, 0, &color4[0], sizeof(glm::vec4));

    SDL_DrawGPUPrimitives(ctx->render_pass, num_sphere_vertices_, 1, 0, 0);
    */

    DrawTargets(view_projection, theme, targets, ctx);
  }

 private:
  void DrawTargets(const glm::mat4& view_projection,
                   const Theme& theme,
                   const std::vector<Target>& targets,
                   RenderContext* ctx) {
    bool has_spheres = false;
    for (const Target& target : targets) {
      if (target.ShouldDraw()) {
        has_spheres = true;
      }
    }
    if (!has_spheres) {
      return;
    }
    glm::vec3 target_color = theme.has_target_color() ? ToVec3(theme.target_color()) : glm::vec3(0);
    glm::vec3 ghost_target_color =
        theme.has_ghost_target_color() ? ToVec3(theme.ghost_target_color()) : glm::vec3(0.3);

    SDL_BindGPUGraphicsPipeline(ctx->render_pass, sphere_pipeline_);
    SDL_GPUBufferBinding binding{};
    binding.buffer = sphere_vertex_buffer_;
    binding.offset = 0;
    SDL_BindGPUVertexBuffers(ctx->render_pass, 0, &binding, 1);

    for (const Target& target : targets) {
      if (target.ShouldDraw()) {
        glm::vec3 color = target.is_ghost ? ghost_target_color : target_color;
        if (target.is_pill) {
          Cylinder c;
          c.radius = target.radius;
          c.up = target.pill_up;
          c.height = target.height - target.radius;
          c.position = target.position;
          // cylinder_renderer_.Draw(ToVec3(room.camera_position()), view, color, {c});

          DrawSphere(
              view_projection, c.position + c.up * (c.height * 0.5f), target.radius, color, ctx);
          DrawSphere(
              view_projection, c.position + c.up * (c.height * -0.5f), target.radius, color, ctx);
        } else {
          DrawSphere(view_projection, target.position, target.radius, color, ctx);
        }
      }
    }
  }

  void DrawSphere(const glm::mat4& view_projection,
                  const glm::vec3& position,
                  float radius,
                  const glm::vec3& color,
                  RenderContext* ctx) {
    glm::mat4 transform(1.0f);
    transform = glm::translate(transform, position);
    transform = glm::scale(transform, glm::vec3(radius));
    transform = view_projection * transform;
    SDL_PushGPUVertexUniformData(ctx->command_buffer, 0, &transform[0][0], sizeof(glm::mat4));

    glm::vec4 color4(color, 1.0f);
    SDL_PushGPUFragmentUniformData(ctx->command_buffer, 0, &color4[0], sizeof(glm::vec4));

    SDL_DrawGPUPrimitives(ctx->render_pass, num_sphere_vertices_, 1, 0, 0);
  }

  bool CreateSpherePipeline() {
    SDL_GPUColorTargetDescription color_target_desc[1];
    color_target_desc[0].format = SDL_GetGPUSwapchainTextureFormat(device_, sdl_window_);
    color_target_desc[0].blend_state = DefaultBlendState();

    SDL_GPUGraphicsPipelineTargetInfo target_info = {};
    target_info.num_color_targets = 1;
    target_info.color_target_descriptions = color_target_desc;

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.target_info = target_info;
    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeline_info.vertex_shader = solid_color_vertex_shader_;
    pipeline_info.fragment_shader = solid_color_fragment_shader_;
    pipeline_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;

    SDL_GPUVertexBufferDescription vertex_buffer_descriptions[1];
    vertex_buffer_descriptions[0].slot = 0;
    vertex_buffer_descriptions[0].pitch = sizeof(float) * 3;
    vertex_buffer_descriptions[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vertex_buffer_descriptions[0].instance_step_rate = 0;

    pipeline_info.vertex_input_state.num_vertex_buffers = 1;
    pipeline_info.vertex_input_state.vertex_buffer_descriptions = vertex_buffer_descriptions;

    SDL_GPUVertexAttribute vertex_attributes[6];
    vertex_attributes[0].location = 0;
    vertex_attributes[0].buffer_slot = 0;
    vertex_attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertex_attributes[0].offset = 0;

    pipeline_info.vertex_input_state.num_vertex_attributes = 1;
    pipeline_info.vertex_input_state.vertex_attributes = vertex_attributes;

    sphere_pipeline_ = SDL_CreateGPUGraphicsPipeline(device_, &pipeline_info);
    if (sphere_pipeline_ == NULL) {
      Logger::get()->error("ERROR: SpherePipeline SDL_CreateGPUGraphicsPipeline failed: {}",
                           SDL_GetError());
      return false;
    }

    return true;
  }

  SDL_GPUTransferBuffer* CreateSphereVertexBuffer(SDL_GPUCopyPass* copy_pass) {
    std::vector<float> sphere_vertices = GenerateSphereVertices(3);
    num_sphere_vertices_ = sphere_vertices.size() / 3;

    int size = sizeof(float) * sphere_vertices.size();

    SDL_GPUBufferCreateInfo vertex_buffer_create_info{};
    vertex_buffer_create_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vertex_buffer_create_info.size = size;

    SDL_GPUBuffer* vertex_buffer = SDL_CreateGPUBuffer(device_, &vertex_buffer_create_info);

    // Set up buffer data
    SDL_GPUTransferBufferCreateInfo transfer_buffer_create_info{};
    transfer_buffer_create_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_buffer_create_info.size = size;
    SDL_GPUTransferBuffer* transfer_buffer =
        SDL_CreateGPUTransferBuffer(device_, &transfer_buffer_create_info);

    float* transfer_data = (float*)SDL_MapGPUTransferBuffer(device_, transfer_buffer, false);
    SDL_memcpy(transfer_data, sphere_vertices.data(), size);
    SDL_UnmapGPUTransferBuffer(device_, transfer_buffer);

    SDL_GPUTransferBufferLocation location{};
    location.transfer_buffer = transfer_buffer;
    location.offset = 0;
    SDL_GPUBufferRegion region{};
    region.buffer = vertex_buffer;
    region.offset = 0;
    region.size = size;
    SDL_UploadToGPUBuffer(copy_pass, &location, &region, false);

    sphere_vertex_buffer_ = vertex_buffer;
    return transfer_buffer;
  }

  SDL_GPUShader* solid_color_fragment_shader_ = nullptr;
  SDL_GPUShader* solid_color_vertex_shader_ = nullptr;
  SDL_GPUGraphicsPipeline* sphere_pipeline_;
  TextureManager texture_manager_;
  SDL_GPUDevice* device_ = nullptr;
  SDL_Window* sdl_window_ = nullptr;

  SDL_GPUBuffer* sphere_vertex_buffer_ = nullptr;
  unsigned int num_sphere_vertices_;
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
                                         SDL_GPUDevice* device,
                                         SDL_Window* sdl_window) {
  auto renderer = std::make_unique<RendererImpl>(texture_dirs, device, sdl_window);
  if (!renderer->Initialize(shader_dir)) {
    return {};
  }
  return std::move(renderer);
}

}  // namespace aim
