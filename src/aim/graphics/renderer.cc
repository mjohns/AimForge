#include "renderer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

#include "aim/common/geometry.h"
#include "aim/common/log.h"
#include "aim/common/simple_types.h"
#include "aim/common/util.h"
#include "aim/graphics/shapes.h"

namespace aim {
namespace {

struct TexScaleAndTransform {
  glm::vec4 tex_scale{};
  glm::mat4 transform{};
};

constexpr const int kQuadNumVertices = 6;
constexpr const float kMaxDistance = 500.0f;

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
      : texture_manager_(texture_dirs, device), device_(device), sdl_window_(sdl_window) {}

  ~RendererImpl() override {
    Cleanup();
  }

  void Cleanup() override {
    CleanupShaders();
    if (sphere_pipeline_ != nullptr) {
      SDL_ReleaseGPUGraphicsPipeline(device_, sphere_pipeline_);
      sphere_pipeline_ = nullptr;
    }
    if (solid_quad_pipeline_ != nullptr) {
      SDL_ReleaseGPUGraphicsPipeline(device_, solid_quad_pipeline_);
      solid_quad_pipeline_ = nullptr;
    }
    if (texture_quad_pipeline_ != nullptr) {
      SDL_ReleaseGPUGraphicsPipeline(device_, texture_quad_pipeline_);
      texture_quad_pipeline_ = nullptr;
    }
    if (quad_vertex_buffer_ != nullptr) {
      SDL_ReleaseGPUBuffer(device_, quad_vertex_buffer_);
      quad_vertex_buffer_ = nullptr;
    }
    if (cylinder_wall_vertex_buffer_ != nullptr) {
      SDL_ReleaseGPUBuffer(device_, cylinder_wall_vertex_buffer_);
      cylinder_wall_vertex_buffer_ = nullptr;
    }
    if (cylinder_vertex_buffer_ != nullptr) {
      SDL_ReleaseGPUBuffer(device_, cylinder_vertex_buffer_);
      cylinder_vertex_buffer_ = nullptr;
    }
    if (sphere_vertex_buffer_ != nullptr) {
      SDL_ReleaseGPUBuffer(device_, sphere_vertex_buffer_);
      sphere_vertex_buffer_ = nullptr;
    }
    texture_manager_.clear();
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
    if (position_and_tex_coord_vertex_shader_ != nullptr) {
      SDL_ReleaseGPUShader(device_, position_and_tex_coord_vertex_shader_);
      position_and_tex_coord_vertex_shader_ = nullptr;
    }
    if (texture_fragment_shader_ != nullptr) {
      SDL_ReleaseGPUShader(device_, texture_fragment_shader_);
      texture_fragment_shader_ = nullptr;
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
    texture_fragment_shader_ = LoadShader(device_,
                                          (shader_dir / "texture.frag.spv").string().c_str(),
                                          SDL_GPU_SHADERSTAGE_FRAGMENT,
                                          1,
                                          1);
    position_and_tex_coord_vertex_shader_ =
        LoadShader(device_,
                   (shader_dir / "position_and_texture_coord.vert.spv").string().c_str(),
                   SDL_GPU_SHADERSTAGE_VERTEX,
                   1);

    if (!CreateSpherePipeline()) {
      return false;
    }
    if (!CreateSolidQuadPipeline()) {
      return false;
    }
    if (!CreateTextureQuadPipeline()) {
      return false;
    }

    SDL_GPUCommandBuffer* upload_command_buffer = SDL_AcquireGPUCommandBuffer(device_);
    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(upload_command_buffer);

    SDL_GPUTransferBuffer* sphere_transfer_buffer = CreateSphereVertexBuffer(copy_pass);
    SDL_GPUTransferBuffer* quad_transfer_buffer = CreateQuadVertexBuffer(copy_pass);
    SDL_GPUTransferBuffer* cylinder_wall_transfer_buffer =
        CreateCylinderWallVertexBuffer(copy_pass);
    SDL_GPUTransferBuffer* cylinder_transfer_buffer = CreateCylinderVertexBuffer(copy_pass);

    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(upload_command_buffer);
    SDL_ReleaseGPUTransferBuffer(device_, sphere_transfer_buffer);
    SDL_ReleaseGPUTransferBuffer(device_, quad_transfer_buffer);
    SDL_ReleaseGPUTransferBuffer(device_, cylinder_wall_transfer_buffer);
    SDL_ReleaseGPUTransferBuffer(device_, cylinder_transfer_buffer);

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
    times->render_room_start = stopwatch.GetElapsedMicros();
    DrawRoom(view_projection, theme, room, ctx);
    times->render_room_end = stopwatch.GetElapsedMicros();

    times->render_targets_start = stopwatch.GetElapsedMicros();
    DrawTargets(view_projection, theme, targets, ctx);
    times->render_targets_end = stopwatch.GetElapsedMicros();
  }

 private:
  void DrawRoom(const glm::mat4& view_projection,
                const Theme& theme,
                const Room& room,
                RenderContext* ctx) {
    if (room.has_simple_room()) {
      DrawSimpleRoom(view_projection, theme, room.simple_room(), ctx);
    }
    if (room.has_cylinder_room()) {
      DrawCylinderRoom(view_projection, theme, room.cylinder_room(), ctx);
    }
    if (room.has_barrel_room()) {
      DrawBarrelRoom(view_projection, theme, room.barrel_room(), ctx);
    }
  }

  void DrawSimpleRoom(const glm::mat4& view_projection,
                      const Theme& theme,
                      const SimpleRoom& room,
                      RenderContext* ctx) {
    float height = room.height();
    float width = room.width();

    float depth = room.has_depth() ? room.depth() : kMaxDistance;
    bool not_cylinder = false;

    {
      // Front wall
      glm::mat4 model(1.f);
      model = glm::scale(model, glm::vec3(width, 1.0f, height));
      DrawWall(
          view_projection * model, {width, height}, theme.front_appearance(), not_cylinder, ctx);
    }

    /*
    {
      // Back wall
      glm::mat4 model(1.f);
      model = glm::translate(model, glm::vec3(0, -1 * depth, 0));
      model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0, 0, 1));
      model = glm::scale(model, glm::vec3(width, 1.0f, height));
      DrawWall(model,
               view,
               {width, height},
               theme.has_back_appearance() ? theme.back_appearance() : theme.front_appearance());
    }
    */

    {
      // Floor wall
      glm::mat4 model(1.f);
      model = glm::translate(model, glm::vec3(0, -0.5 * depth, -0.5 * height));
      model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1, 0, 0));
      model = glm::scale(model, glm::vec3(width, 1.0f, depth));
      DrawWall(
          view_projection * model, {width, depth}, theme.floor_appearance(), not_cylinder, ctx);
    }

    {
      // Left wall
      glm::mat4 model(1.f);
      model = glm::translate(model, glm::vec3(-0.5 * width, -0.5 * depth, 0));
      model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0, 0, 1));
      model = glm::scale(model, glm::vec3(depth, 1.0f, height));
      DrawWall(
          view_projection * model, {depth, height}, theme.side_appearance(), not_cylinder, ctx);
    }

    {
      // Right wall
      glm::mat4 model(1.f);
      model = glm::translate(model, glm::vec3(0.5 * width, -0.5 * depth, 0));
      model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(0, 0, 1));
      model = glm::scale(model, glm::vec3(depth, 1.0f, height));
      DrawWall(
          view_projection * model, {depth, height}, theme.side_appearance(), not_cylinder, ctx);
    }

    {
      // Top wall
      glm::mat4 model(1.f);
      model = glm::translate(model, glm::vec3(0, -0.5 * depth, 0.5 * height));
      model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1, 0, 0));
      model = glm::scale(model, glm::vec3(width, 1.0f, depth));
      DrawWall(view_projection * model, {width, depth}, theme.roof_appearance(), not_cylinder, ctx);
    }
  }

  void DrawCylinderRoom(const glm::mat4& view_projection,
                        const Theme& theme,
                        const CylinderRoom& room,
                        RenderContext* ctx) {
    float quad_scale = room.radius() * 2.5;
    float height = room.height();

    {
      // Floor wall
      glm::mat4 model(1.f);
      model = glm::translate(model, glm::vec3(0, 0, -0.5 * height));
      model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1, 0, 0));
      model = glm::scale(model, glm::vec3(quad_scale, 1.0f, quad_scale));
      DrawWall(
          view_projection * model, {quad_scale, quad_scale}, theme.floor_appearance(), false, ctx);
    }

    {
      // Top wall
      glm::mat4 model(1.f);
      model = glm::translate(model, glm::vec3(0, 0, 0.5 * height));
      model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1, 0, 0));
      model = glm::scale(model, glm::vec3(quad_scale, 1.0f, quad_scale));
      DrawWall(
          view_projection * model, {quad_scale, quad_scale}, theme.roof_appearance(), false, ctx);
    }

    if (!room.hide_sides()) {
      float width = room.width();
      if (room.has_width_perimeter_percent()) {
        width = room.width_perimeter_percent() * glm::two_pi<float>() * room.radius();
      }
      float perimeter = room.radius() * glm::two_pi<float>();

      float radians = (width / perimeter) * glm::pi<float>();
      glm::vec2 to_rotate(0, room.radius());
      float side_angle_degrees = room.has_side_angle_degrees() ? room.side_angle_degrees() : 20.0f;

      {
        // Left
        glm::vec2 left = RotateRadians(to_rotate, radians);
        glm::mat4 model(1.f);
        model = glm::translate(model, glm::vec3(left.x, left.y, 0));
        model = glm::rotate(model, glm::radians(90.0f + side_angle_degrees), glm::vec3(0, 0, 1));
        model = glm::translate(model, glm::vec3(-0.5 * kMaxDistance, 0, 0));
        model = glm::scale(model, glm::vec3(kMaxDistance, 1.0f, height));
        DrawWall(
            view_projection * model, {kMaxDistance, height}, theme.side_appearance(), false, ctx);
      }

      {
        // Right
        glm::vec2 right = RotateRadians(to_rotate, -1 * radians);
        glm::mat4 model(1.f);
        model = glm::translate(model, glm::vec3(right.x, right.y, 0));
        model = glm::rotate(model, glm::radians(-90.0f - side_angle_degrees), glm::vec3(0, 0, 1));
        model = glm::translate(model, glm::vec3(0.5 * kMaxDistance, 0, 0));
        model = glm::scale(model, glm::vec3(kMaxDistance, 1.0f, height));
        DrawWall(
            view_projection * model, {kMaxDistance, height}, theme.side_appearance(), false, ctx);
      }
    }

    {
      glm::mat4 model(1.f);
      model = glm::scale(model, glm::vec3(room.radius(), room.radius(), height));
      DrawWall(view_projection * model,
               {glm::two_pi<float>() * room.radius(), height},
               theme.front_appearance(),
               /* is_cylinder_wall= */ true,
               ctx);
    }
  }

  void DrawBarrelRoom(const glm::mat4& view_projection,
                      const Theme& theme,
                      const BarrelRoom& room,
                      RenderContext* ctx) {
    float quad_scale = room.radius() * 2.1;

    {
      // Front wall
      glm::mat4 model(1.f);
      model = glm::scale(model, glm::vec3(quad_scale, 1.0f, quad_scale));
      DrawWall(
          view_projection * model, {quad_scale, quad_scale}, theme.front_appearance(), false, ctx);
    }
    /*
{
  // Back wall
  glm::mat4 model(1.f);
  model = glm::translate(model, glm::vec3(0, -1 * kMaxDistance, 0));
  model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0, 0, 1));
  model = glm::scale(model, glm::vec3(quad_scale, 1.0f, quad_scale));
  DrawWall(model,
           view,
           {quad_scale, quad_scale},
           theme.has_back_appearance() ? theme.back_appearance() : theme.front_appearance());
}
*/

    {
      glm::mat4 model(1.f);
      model = glm::translate(model, glm::vec3(0, -0.5 * kMaxDistance, 0));
      model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1, 0, 0));
      model = glm::scale(model, glm::vec3(room.radius(), room.radius(), kMaxDistance));
      DrawWall(view_projection * model,
               {glm::two_pi<float>() * room.radius(), kMaxDistance},
               theme.side_appearance(),
               /* is_cylinder_wall= */ true,
               ctx);
    }
  }

  void DrawWall(const glm::mat4& transform,
                const Wall& wall,
                const WallAppearance& appearance,
                bool is_cylinder_wall,
                RenderContext* ctx) {
    if (appearance.has_texture()) {
      Texture* texture = texture_manager_.GetTexture(appearance.texture().texture_name());
      if (texture == nullptr) {
        // Too spammy to log this error?
        DrawWallSolidColor(transform, glm::vec3(0.7), is_cylinder_wall, ctx);
        return;
      }

      glm::vec2 tex_scale;

      float tex_scale_height = 100;
      float tex_scale_width = (texture->width() * tex_scale_height) / (float)texture->height();

      tex_scale.x = wall.width / tex_scale_width;
      tex_scale.y = wall.height / tex_scale_height;

      if (appearance.texture().has_scale()) {
        tex_scale *= appearance.texture().scale();
      }

      SDL_BindGPUGraphicsPipeline(ctx->render_pass, texture_quad_pipeline_);
      SDL_GPUBufferBinding binding{};
      binding.buffer = is_cylinder_wall ? cylinder_wall_vertex_buffer_ : quad_vertex_buffer_;
      binding.offset = 0;
      SDL_BindGPUVertexBuffers(ctx->render_pass, 0, &binding, 1);

      TexScaleAndTransform tex_scale_and_transform{};
      tex_scale_and_transform.tex_scale.x = tex_scale.x;
      tex_scale_and_transform.tex_scale.y = tex_scale.y;
      tex_scale_and_transform.transform = transform;

      SDL_PushGPUVertexUniformData(ctx->command_buffer,
                                   0,
                                   &tex_scale_and_transform.tex_scale[0],
                                   sizeof(TexScaleAndTransform));

      glm::vec3 mix_color = ToVec3(appearance.texture().mix_color());
      glm::vec4 color4(mix_color, appearance.texture().mix_percent());
      SDL_PushGPUFragmentUniformData(ctx->command_buffer, 0, &color4[0], sizeof(glm::vec4));
      SDL_BindGPUFragmentSamplers(ctx->render_pass, 0, texture->texture_sampler_binding(), 1);

      SDL_DrawGPUPrimitives(ctx->render_pass,
                            is_cylinder_wall ? num_cylinder_wall_vertices_ : kQuadNumVertices,
                            1,
                            0,
                            0);
      return;
    }

    DrawWallSolidColor(transform, ToVec3(appearance.color()), is_cylinder_wall, ctx);
  }

  void DrawWallSolidColor(const glm::mat4& transform,
                          const glm::vec3& color,
                          bool is_cylinder_wall,
                          RenderContext* ctx) {
    SDL_BindGPUGraphicsPipeline(ctx->render_pass, solid_quad_pipeline_);
    SDL_GPUBufferBinding binding{};
    binding.buffer = is_cylinder_wall ? cylinder_wall_vertex_buffer_ : quad_vertex_buffer_;
    binding.offset = 0;
    SDL_BindGPUVertexBuffers(ctx->render_pass, 0, &binding, 1);
    SDL_PushGPUVertexUniformData(ctx->command_buffer, 0, &transform[0][0], sizeof(glm::mat4));

    glm::vec4 color4(color, 1.0f);
    SDL_PushGPUFragmentUniformData(ctx->command_buffer, 0, &color4[0], sizeof(glm::vec4));

    SDL_DrawGPUPrimitives(ctx->render_pass,
                          is_cylinder_wall ? num_cylinder_wall_vertices_ : kQuadNumVertices,
                          1,
                          0,
                          0);
  }

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

    for (const Target& target : targets) {
      if (target.ShouldDraw()) {
        glm::vec3 color = target.is_ghost ? ghost_target_color : target_color;
        if (target.is_pill) {
          Cylinder c;
          c.radius = target.radius;
          c.up = target.pill_up;
          c.height = target.height - target.radius;
          c.position = target.position;
          DrawCylinder(view_projection, c, color, ctx);

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
    SDL_GPUBufferBinding binding{};
    binding.buffer = sphere_vertex_buffer_;
    binding.offset = 0;
    SDL_BindGPUVertexBuffers(ctx->render_pass, 0, &binding, 1);

    glm::mat4 transform(1.0f);
    transform = glm::translate(transform, position);
    transform = glm::scale(transform, glm::vec3(radius));
    transform = view_projection * transform;
    SDL_PushGPUVertexUniformData(ctx->command_buffer, 0, &transform[0][0], sizeof(glm::mat4));

    glm::vec4 color4(color, 1.0f);
    SDL_PushGPUFragmentUniformData(ctx->command_buffer, 0, &color4[0], sizeof(glm::vec4));

    SDL_DrawGPUPrimitives(ctx->render_pass, num_sphere_vertices_, 1, 0, 0);
  }

  void DrawCylinder(const glm::mat4& view_projection,
                    const Cylinder& c,
                    const glm::vec3& color,
                    RenderContext* ctx) {
    SDL_GPUBufferBinding binding{};
    binding.buffer = cylinder_vertex_buffer_;
    binding.offset = 0;
    SDL_BindGPUVertexBuffers(ctx->render_pass, 0, &binding, 1);

    glm::mat4 model(1.0f);
    model = glm::translate(model, c.position);
    if (c.up != glm::vec3(0, 0, 1) && c.up != glm::vec3(0, 0, -1)) {
      glm::vec3 up = glm::vec3(0, 0, 1);
      glm::vec3 rotate_axis = glm::normalize(glm::cross(up, c.up));
      float angle = glm::acos(glm::dot(up, c.up));
      model = glm::rotate(model, angle, rotate_axis);
    }
    model = glm::scale(model, glm::vec3(c.radius, c.radius, c.height));

    glm::mat4 transform = view_projection * model;
    SDL_PushGPUVertexUniformData(ctx->command_buffer, 0, &transform[0][0], sizeof(glm::mat4));

    glm::vec4 color4(color, 1.0f);
    SDL_PushGPUFragmentUniformData(ctx->command_buffer, 0, &color4[0], sizeof(glm::vec4));

    SDL_DrawGPUPrimitives(ctx->render_pass, num_cylinder_vertices_, 1, 0, 0);
  }

  bool CreateSpherePipeline() {
    SDL_GPUColorTargetDescription color_target_desc[1];
    color_target_desc[0].format = SDL_GetGPUSwapchainTextureFormat(device_, sdl_window_);
    color_target_desc[0].blend_state = DefaultBlendState();

    SDL_GPUGraphicsPipelineTargetInfo target_info = {};
    target_info.num_color_targets = 1;
    target_info.color_target_descriptions = color_target_desc;
    target_info.has_depth_stencil_target = true;
    target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.target_info = target_info;
    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeline_info.vertex_shader = solid_color_vertex_shader_;
    pipeline_info.fragment_shader = solid_color_fragment_shader_;
    pipeline_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipeline_info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    pipeline_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    SDL_GPUVertexBufferDescription vertex_buffer_descriptions[1];
    vertex_buffer_descriptions[0].slot = 0;
    vertex_buffer_descriptions[0].pitch = sizeof(float) * 3;
    vertex_buffer_descriptions[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vertex_buffer_descriptions[0].instance_step_rate = 0;

    pipeline_info.vertex_input_state.num_vertex_buffers = 1;
    pipeline_info.vertex_input_state.vertex_buffer_descriptions = vertex_buffer_descriptions;

    SDL_GPUVertexAttribute vertex_attributes[1];
    vertex_attributes[0].location = 0;
    vertex_attributes[0].buffer_slot = 0;
    vertex_attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertex_attributes[0].offset = 0;

    pipeline_info.vertex_input_state.num_vertex_attributes = 1;
    pipeline_info.vertex_input_state.vertex_attributes = vertex_attributes;

    SDL_GPUDepthStencilState depth_stencil_state{};
    depth_stencil_state.enable_depth_test = true;
    depth_stencil_state.enable_depth_write = true;
    depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    depth_stencil_state.write_mask = 0xFF;

    sphere_pipeline_ = SDL_CreateGPUGraphicsPipeline(device_, &pipeline_info);
    if (sphere_pipeline_ == NULL) {
      Logger::get()->error("ERROR: SpherePipeline SDL_CreateGPUGraphicsPipeline failed: {}",
                           SDL_GetError());
      return false;
    }

    return true;
  }

  bool CreateSolidQuadPipeline() {
    SDL_GPUColorTargetDescription color_target_desc[1];
    color_target_desc[0].format = SDL_GetGPUSwapchainTextureFormat(device_, sdl_window_);
    color_target_desc[0].blend_state = DefaultBlendState();

    SDL_GPUGraphicsPipelineTargetInfo target_info = {};
    target_info.num_color_targets = 1;
    target_info.color_target_descriptions = color_target_desc;
    target_info.has_depth_stencil_target = true;
    target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.target_info = target_info;
    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeline_info.vertex_shader = solid_color_vertex_shader_;
    pipeline_info.fragment_shader = solid_color_fragment_shader_;
    pipeline_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipeline_info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    pipeline_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    SDL_GPUVertexBufferDescription vertex_buffer_descriptions[1];
    vertex_buffer_descriptions[0].slot = 0;
    vertex_buffer_descriptions[0].pitch = sizeof(VertexAndTexCoord);
    vertex_buffer_descriptions[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vertex_buffer_descriptions[0].instance_step_rate = 0;

    pipeline_info.vertex_input_state.num_vertex_buffers = 1;
    pipeline_info.vertex_input_state.vertex_buffer_descriptions = vertex_buffer_descriptions;

    SDL_GPUVertexAttribute vertex_attributes[1];
    vertex_attributes[0].location = 0;
    vertex_attributes[0].buffer_slot = 0;
    vertex_attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertex_attributes[0].offset = 0;

    pipeline_info.vertex_input_state.num_vertex_attributes = 1;
    pipeline_info.vertex_input_state.vertex_attributes = vertex_attributes;

    SDL_GPUDepthStencilState depth_stencil_state{};
    depth_stencil_state.enable_depth_test = true;
    depth_stencil_state.enable_depth_write = true;
    depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    depth_stencil_state.write_mask = 0xFF;
    pipeline_info.depth_stencil_state = depth_stencil_state;

    solid_quad_pipeline_ = SDL_CreateGPUGraphicsPipeline(device_, &pipeline_info);
    if (solid_quad_pipeline_ == nullptr) {
      Logger::get()->error("ERROR: SolidQuadPipeline SDL_CreateGPUGraphicsPipeline failed: {}",
                           SDL_GetError());
      return false;
    }

    return true;
  }

  bool CreateTextureQuadPipeline() {
    SDL_GPUColorTargetDescription color_target_desc[1];
    color_target_desc[0].format = SDL_GetGPUSwapchainTextureFormat(device_, sdl_window_);
    color_target_desc[0].blend_state = DefaultBlendState();

    SDL_GPUGraphicsPipelineTargetInfo target_info = {};
    target_info.num_color_targets = 1;
    target_info.color_target_descriptions = color_target_desc;
    target_info.has_depth_stencil_target = true;
    target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.target_info = target_info;
    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeline_info.vertex_shader = position_and_tex_coord_vertex_shader_;
    pipeline_info.fragment_shader = texture_fragment_shader_;
    pipeline_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipeline_info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    pipeline_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    SDL_GPUVertexBufferDescription vertex_buffer_descriptions[1];
    vertex_buffer_descriptions[0].slot = 0;
    vertex_buffer_descriptions[0].pitch = sizeof(VertexAndTexCoord);
    vertex_buffer_descriptions[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vertex_buffer_descriptions[0].instance_step_rate = 0;

    pipeline_info.vertex_input_state.num_vertex_buffers = 1;
    pipeline_info.vertex_input_state.vertex_buffer_descriptions = vertex_buffer_descriptions;

    SDL_GPUVertexAttribute vertex_attributes[2];
    vertex_attributes[0].location = 0;
    vertex_attributes[0].buffer_slot = 0;
    vertex_attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertex_attributes[0].offset = 0;

    vertex_attributes[1].location = 1;
    vertex_attributes[1].buffer_slot = 0;
    vertex_attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    vertex_attributes[1].offset = sizeof(float) * 3;

    pipeline_info.vertex_input_state.num_vertex_attributes = 2;
    pipeline_info.vertex_input_state.vertex_attributes = vertex_attributes;

    SDL_GPUDepthStencilState depth_stencil_state{};
    depth_stencil_state.enable_depth_test = true;
    depth_stencil_state.enable_depth_write = true;
    depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    depth_stencil_state.write_mask = 0xFF;
    pipeline_info.depth_stencil_state = depth_stencil_state;

    texture_quad_pipeline_ = SDL_CreateGPUGraphicsPipeline(device_, &pipeline_info);
    if (texture_quad_pipeline_ == nullptr) {
      Logger::get()->error("ERROR: TextureQuadPipeline SDL_CreateGPUGraphicsPipeline failed: {}",
                           SDL_GetError());
      return false;
    }

    return true;
  }

  SDL_GPUTransferBuffer* CreateSphereVertexBuffer(SDL_GPUCopyPass* copy_pass) {
    std::vector<float> sphere_vertices = GenerateSphereVertices(3);
    num_sphere_vertices_ = sphere_vertices.size() / 3;
    int size = sizeof(float) * sphere_vertices.size();
    return UploadBuffer(sphere_vertices.data(), size, copy_pass, &sphere_vertex_buffer_);
  }

  SDL_GPUTransferBuffer* CreateQuadVertexBuffer(SDL_GPUCopyPass* copy_pass) {
    VertexAndTexCoord bottom_right;
    VertexAndTexCoord bottom_left;
    VertexAndTexCoord top_left;
    VertexAndTexCoord top_right;

    bottom_right.vertex = glm::vec3(0.5f, 0.0f, -0.5f);
    bottom_left.vertex = glm::vec3(-0.5f, 0.0f, -0.5f);
    top_left.vertex = glm::vec3(-0.5f, 0.0f, 0.5f);
    top_right.vertex = glm::vec3(0.5f, 0.0f, 0.5f);

    bottom_right.tex_coord = glm::vec2(1.0f, 1.0f);
    bottom_left.tex_coord = glm::vec2(0.0f, 1.0f);
    top_left.tex_coord = glm::vec2(0.0f, 0.0f);
    top_right.tex_coord = glm::vec2(1.0f, 0.0f);

    std::vector<VertexAndTexCoord> vertices = {
        bottom_right,
        top_right,
        top_left,
        top_left,
        bottom_left,
        bottom_right,
    };

    int size = sizeof(VertexAndTexCoord) * vertices.size();
    return UploadBuffer(vertices.data(), size, copy_pass, &quad_vertex_buffer_);
  }

  SDL_GPUTransferBuffer* CreateCylinderWallVertexBuffer(SDL_GPUCopyPass* copy_pass) {
    std::vector<VertexAndTexCoord> vertices = GenerateCylinderWallVertices(400);
    num_cylinder_wall_vertices_ = vertices.size();
    int size = sizeof(VertexAndTexCoord) * vertices.size();
    return UploadBuffer(vertices.data(), size, copy_pass, &cylinder_wall_vertex_buffer_);
  }

  SDL_GPUTransferBuffer* CreateCylinderVertexBuffer(SDL_GPUCopyPass* copy_pass) {
    std::vector<glm::vec3> vertices = GenerateCylinderVertices(100);
    num_cylinder_vertices_ = vertices.size();
    int size = sizeof(glm::vec3) * vertices.size();
    return UploadBuffer(vertices.data(), size, copy_pass, &cylinder_vertex_buffer_);
  }

  SDL_GPUTransferBuffer* UploadBuffer(void* data,
                                      int size,
                                      SDL_GPUCopyPass* copy_pass,
                                      SDL_GPUBuffer** vertex_buffer_out) {
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

    void* transfer_data = (void*)SDL_MapGPUTransferBuffer(device_, transfer_buffer, false);
    SDL_memcpy(transfer_data, data, size);
    SDL_UnmapGPUTransferBuffer(device_, transfer_buffer);

    SDL_GPUTransferBufferLocation location{};
    location.transfer_buffer = transfer_buffer;
    location.offset = 0;
    SDL_GPUBufferRegion region{};
    region.buffer = vertex_buffer;
    region.offset = 0;
    region.size = size;
    SDL_UploadToGPUBuffer(copy_pass, &location, &region, false);

    *vertex_buffer_out = vertex_buffer;
    return transfer_buffer;
  }

  SDL_GPUShader* solid_color_fragment_shader_ = nullptr;
  SDL_GPUShader* solid_color_vertex_shader_ = nullptr;
  SDL_GPUShader* position_and_tex_coord_vertex_shader_ = nullptr;
  SDL_GPUShader* texture_fragment_shader_ = nullptr;
  SDL_GPUGraphicsPipeline* sphere_pipeline_;
  SDL_GPUGraphicsPipeline* solid_quad_pipeline_;
  SDL_GPUGraphicsPipeline* texture_quad_pipeline_;
  TextureManager texture_manager_;
  SDL_GPUDevice* device_ = nullptr;
  SDL_Window* sdl_window_ = nullptr;

  SDL_GPUBuffer* quad_vertex_buffer_ = nullptr;
  SDL_GPUBuffer* cylinder_wall_vertex_buffer_ = nullptr;
  SDL_GPUBuffer* cylinder_vertex_buffer_ = nullptr;
  SDL_GPUBuffer* sphere_vertex_buffer_ = nullptr;
  unsigned int num_sphere_vertices_;
  unsigned int num_cylinder_wall_vertices_;
  unsigned int num_cylinder_vertices_;
};

}  // namespace

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
