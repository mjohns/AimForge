#include "renderer.h"

#include "SDL3/SDL_gpu.h"

#define GLM_ENABLE_EXPERIMENTAL

#include <cassert>

#include "SDL3/SDL.h"  // IWYU pragma: keep
#include "absl/algorithm/container.h"
#include "aim/common/geometry.h"
#include "aim/common/log.h"
#include "aim/common/simple_types.h"
#include "aim/common/util.h"
#include "aim/graphics/shapes.h"
#include "aim/graphics/textures.h"
#include "glm/gtc/matrix_transform.hpp"  // IWYU pragma: keep
#include "glm/gtx/vector_angle.hpp"
#include "glm/mat4x4.hpp"  // IWYU pragma: keep
#include "imgui/backends/imgui_impl_sdlgpu3.h"

namespace aim {
namespace {

constexpr const int kQuadNumVertices = 6;
constexpr const float kMaxDistance = 1500.0f;
constexpr const u32 kMaxSolidColorInstances = 1500;
constexpr const float kCenterSphereSizeMultiplier = 0.2f;
constexpr const glm::vec3 kWorldUp = glm::vec3(0, 0, 1);

glm::mat4 RotateTowardsCameraAroundZ(const LookAtInfo& look_at,
                                     const glm::vec3& position,
                                     const glm::mat4& transform) {
  // Rotate to face towards camera along the z axis
  glm::vec3 to_camera = look_at.position - position;
  to_camera.z = 0;
  if (glm::length(to_camera) > 0.001) {
    float angle = glm::orientedAngle(glm::vec3(0, -1, 0), glm::normalize(to_camera), kWorldUp);
    return glm::rotate(transform, angle, kWorldUp);
  }
  return transform;
}

glm::mat4 RotateBasicShapeTowardsCamera(const LookAtInfo& look_at,
                                        const glm::vec3& position,
                                        const glm::mat4& transform,
                                        bool rotate_z) {
  if (look_at.position == position) {
    return transform;
  }
  glm::vec3 to_camera = look_at.position - position;
  if (!rotate_z) {
    to_camera.z = 0;
  }
  // The quad/circle types are defined in the x/z plane facing in the negative y direction.
  if (glm::length(to_camera) < 0.001) {
    return transform;
  }
  to_camera = glm::normalize(to_camera);
  glm::vec3 front(0, -1, 0);
  glm::vec3 rotate_axis = glm::normalize(glm::cross(front, to_camera));
  float angle = glm::orientedAngle(front, to_camera, rotate_axis);
  return glm::rotate(transform, angle, rotate_axis);
}

struct SolidColorInstanceData {
  glm::mat4 transform;
  glm::vec4 color;
};

struct SolidColorInstances {
  std::vector<SolidColorInstanceData> instances;

  // Instances will be packed in the following order and summing num_* equals the size of instances
  // vector.
  u32 num_spheres = 0;
  u32 num_cylinders = 0;
  u32 num_quads = 0;
  u32 num_cylinder_walls = 0;
  u32 num_circles = 0;

  u32 GetSpheresOffset() const {
    return 0;
  }

  u32 GetCylindersOffset() const {
    return num_spheres;
  }

  u32 GetQuadsOffset() const {
    return GetCylindersOffset() + num_cylinders;
  }

  u32 GetCylinderWallsOffset() const {
    return GetQuadsOffset() + num_quads;
  }

  u32 GetCirclesOffset() const {
    return GetCylinderWallsOffset() + num_cylinder_walls;
  }
};

struct TexScaleAndTransform {
  glm::vec4 tex_scale{};
  glm::mat4 transform{};
};

struct TextureWallDrawData {
  glm::mat4 transform;
  glm::vec4 color;
  bool is_cylinder = false;
  glm::vec2 tex_scale;
  Texture* texture;
};

struct SolidColorInstancedUniform {
  u32 instance_offset = 0;
};

struct DrawData {
  std::vector<TextureWallDrawData> texture_walls;

  std::vector<SolidColorInstanceData> solid_spheres;
  std::vector<SolidColorInstanceData> solid_cylinders;
  std::vector<SolidColorInstanceData> solid_quads;
  std::vector<SolidColorInstanceData> solid_cylinder_walls;
  std::vector<SolidColorInstanceData> solid_circles;
};

glm::vec3 Lerp(const glm::vec3& a, const glm::vec3& b, float mix_percent) {
  return a + (mix_percent * (b - a));
}

glm::vec3 GetSolidColor(const WallAppearance& appearance) {
  if (!appearance.has_mix_percent()) {
    return ToVec3(appearance.color());
  }
  return Lerp(ToVec3(appearance.color()), ToVec3(appearance.mix_color()), appearance.mix_percent());
}

SDL_GPUShader* LoadShader(SDL_GPUDevice* device,
                          const std::filesystem::path& shader_dir,
                          const std::string& shader_name,
                          SDL_GPUShaderStage stage,
                          Uint32 uniform_buffer_count = 0,
                          Uint32 sampler_count = 0,
                          Uint32 storage_buffer_count = 0,
                          Uint32 storage_texture_count = 0) {
  const char* entrypoint;
  SDL_GPUShaderFormat backend_formats = SDL_GetGPUShaderFormats(device);
  SDL_GPUShaderFormat format = SDL_GPU_SHADERFORMAT_INVALID;

  std::string shader_extension;
  if (backend_formats & SDL_GPU_SHADERFORMAT_DXIL) {
    shader_extension = "dxil";
    format = SDL_GPU_SHADERFORMAT_DXIL;
    entrypoint = "main";
  } else if (backend_formats & SDL_GPU_SHADERFORMAT_SPIRV) {
    shader_extension = "spv";
    format = SDL_GPU_SHADERFORMAT_SPIRV;
    entrypoint = "main";
  } else if (backend_formats & SDL_GPU_SHADERFORMAT_MSL) {
    shader_extension = "msl";
    format = SDL_GPU_SHADERFORMAT_MSL;
    entrypoint = "main0";
  } else {
    Logger::get()->warn("Unrecognized backend shader format!");
    return nullptr;
  }

  std::filesystem::path shader_path =
      shader_dir / std::format("{}.{}", shader_name, shader_extension);
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

// Turns the scenario scene description into normalized and sorted draw data that can be easily
// rendered.
class DrawDataBuilder {
 public:
  explicit DrawDataBuilder(TextureManager* texture_manager) : texture_manager_(texture_manager) {}

  void GetDrawDataForScenario(const glm::mat4& view_projection,
                              const Room& room,
                              bool draw_center,
                              const Theme& theme,
                              const HealthBarSettings& health_bar,
                              const std::vector<Target>& targets,
                              const LookAtInfo& look_at,
                              DrawData* draw_data) {
    draw_data->solid_quads.reserve(6 + targets.size() * 2);
    draw_data->solid_spheres.reserve(targets.size() * 2);
    draw_data->solid_circles.reserve(targets.size() * 2);
    draw_data->solid_cylinders.reserve(targets.size());
    AddDrawRoom(view_projection, theme, room, draw_data);
    AddDrawTargets(view_projection, look_at, theme, health_bar, targets, draw_center, draw_data);

    // Make sure all textured walls are grouped by Texture* so that the samplers can be bound
    // minimally and don't need to be switched back and forth.
    absl::c_sort(draw_data->texture_walls,
                 [](const TextureWallDrawData& lhs, const TextureWallDrawData& rhs) {
                   return lhs.texture < rhs.texture;
                 });
  }

 private:
  void AddDrawRoom(const glm::mat4& view_projection,
                   const Theme& theme,
                   const Room& room,
                   DrawData* draw_data) {
    if (room.has_simple_room()) {
      AddDrawSimpleRoom(view_projection, theme, room.simple_room(), draw_data);
    }
    if (room.has_cylinder_room()) {
      AddDrawCylinderRoom(view_projection, theme, room.cylinder_room(), draw_data);
    }
    if (room.has_barrel_room()) {
      AddDrawBarrelRoom(view_projection, theme, room.barrel_room(), draw_data);
    }
  }

  void AddDrawSimpleRoom(const glm::mat4& view_projection,
                         const Theme& theme,
                         const SimpleRoom& room,
                         DrawData* draw_data) {
    float height = room.height();
    float width = room.width();

    float depth = room.has_depth() ? room.depth() : kMaxDistance;
    bool not_cylinder = false;

    {
      // Front wall
      glm::mat4 model(1.f);
      model = glm::scale(model, glm::vec3(width, 1.0f, height));
      AddDrawWall(view_projection * model,
                  {width, height},
                  theme.front_appearance(),
                  not_cylinder,
                  draw_data);
    }

    if (room.has_depth()) {
      // Back wall
      glm::mat4 model(1.f);
      model = glm::translate(model, glm::vec3(0, -1 * depth, 0));
      model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0, 0, 1));
      model = glm::scale(model, glm::vec3(width, 1.0f, height));
      AddDrawWall(view_projection * model,
                  {width, height},
                  theme.front_appearance(),
                  not_cylinder,
                  draw_data);
    }

    {
      // Floor wall
      glm::mat4 model(1.f);
      model = glm::translate(model, glm::vec3(0, -0.5 * depth, -0.5 * height));
      model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1, 0, 0));
      model = glm::scale(model, glm::vec3(width, 1.0f, depth));
      AddDrawWall(view_projection * model,
                  {width, depth},
                  theme.floor_appearance(),
                  not_cylinder,
                  draw_data);
    }

    {
      // Left wall
      glm::mat4 model(1.f);
      model = glm::translate(model, glm::vec3(-0.5 * width, -0.5 * depth, 0));
      model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0, 0, 1));
      model = glm::scale(model, glm::vec3(depth, 1.0f, height));
      AddDrawWall(view_projection * model,
                  {depth, height},
                  theme.side_appearance(),
                  not_cylinder,
                  draw_data);
    }

    {
      // Right wall
      glm::mat4 model(1.f);
      model = glm::translate(model, glm::vec3(0.5 * width, -0.5 * depth, 0));
      model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(0, 0, 1));
      model = glm::scale(model, glm::vec3(depth, 1.0f, height));
      AddDrawWall(view_projection * model,
                  {depth, height},
                  theme.side_appearance(),
                  not_cylinder,
                  draw_data);
    }

    {
      // Top wall
      glm::mat4 model(1.f);
      model = glm::translate(model, glm::vec3(0, -0.5 * depth, 0.5 * height));
      model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1, 0, 0));
      model = glm::scale(model, glm::vec3(width, 1.0f, depth));
      AddDrawWall(view_projection * model,
                  {width, depth},
                  theme.roof_appearance(),
                  not_cylinder,
                  draw_data);
    }
  }

  void AddDrawCylinderRoom(const glm::mat4& view_projection,
                           const Theme& theme,
                           const CylinderRoom& room,
                           DrawData* draw_data) {
    float quad_scale = room.radius() * 2.5;
    float height = room.height();

    {
      // Floor wall
      glm::mat4 model(1.f);
      model = glm::translate(model, glm::vec3(0, 0, -0.505 * height));
      model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1, 0, 0));
      model = glm::scale(model, glm::vec3(quad_scale, 1.0f, quad_scale));
      AddDrawWall(view_projection * model,
                  {quad_scale, quad_scale},
                  theme.floor_appearance(),
                  false,
                  draw_data);
    }

    {
      // Top wall
      glm::mat4 model(1.f);
      model = glm::translate(model, glm::vec3(0, 0, 0.505 * height));
      model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1, 0, 0));
      model = glm::scale(model, glm::vec3(quad_scale, 1.0f, quad_scale));
      AddDrawWall(view_projection * model,
                  {quad_scale, quad_scale},
                  theme.roof_appearance(),
                  false,
                  draw_data);
    }

    if (room.has_side_angle_degrees()) {
      float perimeter = room.radius() * glm::two_pi<float>();
      float width = room.width();
      if (room.width_degrees() > 0) {
        width = (room.width_degrees() / 360.0f) * perimeter;
      }

      float radians = (width / perimeter) * glm::pi<float>();
      glm::vec2 to_rotate(0, room.radius());
      float side_angle_degrees = room.side_angle_degrees();

      {
        // Left
        glm::vec2 left = RotateRadians(to_rotate, radians);
        glm::mat4 model(1.f);
        model = glm::translate(model, glm::vec3(left.x, left.y, 0));
        model = glm::rotate(model, glm::radians(90.0f + side_angle_degrees), glm::vec3(0, 0, 1));
        model = glm::translate(model, glm::vec3(-0.5 * kMaxDistance, 0, 0));
        model = glm::scale(model, glm::vec3(kMaxDistance, 1.0f, height));
        AddDrawWall(view_projection * model,
                    {kMaxDistance, height},
                    theme.side_appearance(),
                    false,
                    draw_data);
      }

      {
        // Right
        glm::vec2 right = RotateRadians(to_rotate, -1 * radians);
        glm::mat4 model(1.f);
        model = glm::translate(model, glm::vec3(right.x, right.y, 0));
        model = glm::rotate(model, glm::radians(-90.0f - side_angle_degrees), glm::vec3(0, 0, 1));
        model = glm::translate(model, glm::vec3(0.5 * kMaxDistance, 0, 0));
        model = glm::scale(model, glm::vec3(kMaxDistance, 1.0f, height));
        AddDrawWall(view_projection * model,
                    {kMaxDistance, height},
                    theme.side_appearance(),
                    false,
                    draw_data);
      }
    }

    {
      glm::mat4 model(1.f);
      model = glm::scale(model, glm::vec3(room.radius(), room.radius(), height));
      AddDrawWall(view_projection * model,
                  {glm::two_pi<float>() * room.radius(), height},
                  theme.front_appearance(),
                  /* is_cylinder_wall= */ true,
                  draw_data);
    }
  }

  void AddDrawBarrelRoom(const glm::mat4& view_projection,
                         const Theme& theme,
                         const BarrelRoom& room,
                         DrawData* draw_data) {
    float quad_scale = room.radius() * 100;

    {
      // Front wall
      glm::mat4 model(1.f);
      model = glm::scale(model, glm::vec3(quad_scale, 1.0f, quad_scale));
      AddDrawWall(view_projection * model,
                  {quad_scale, quad_scale},
                  theme.front_appearance(),
                  false,
                  draw_data);
    }

    {
      glm::mat4 model(1.f);
      // Leave a little gap between the wall and the barrel to prevent any z-fighting.
      model = glm::translate(model, glm::vec3(0, -0.5 * kMaxDistance, 0));
      model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1, 0, 0));
      model = glm::scale(model, glm::vec3(room.radius(), room.radius(), kMaxDistance));
      AddDrawWall(view_projection * model,
                  {glm::two_pi<float>() * room.radius(), kMaxDistance},
                  theme.side_appearance(),
                  /* is_cylinder_wall= */ true,
                  draw_data);
    }
  }

  void AddDrawWall(const glm::mat4& transform,
                   const Wall& wall,
                   const WallAppearance& appearance,
                   bool is_cylinder_wall,
                   DrawData* draw_data) {
    if (appearance.has_texture()) {
      Texture* texture = texture_manager_->GetTexture(appearance.texture().texture_name());
      if (texture == nullptr) {
        // Too spammy to log this error?
        AddDrawWallSolidColor(transform, glm::vec3(0.7), is_cylinder_wall, draw_data);
        return;
      }

      TextureWallDrawData data;
      data.is_cylinder = is_cylinder_wall;
      data.transform = transform;
      data.texture = texture;

      glm::vec2& tex_scale = data.tex_scale;

      float tex_scale_height = 100;
      float tex_scale_width = (texture->width() * tex_scale_height) / (float)texture->height();

      tex_scale.x = wall.width / tex_scale_width;
      tex_scale.y = wall.height / tex_scale_height;

      if (appearance.texture().has_scale()) {
        tex_scale *= appearance.texture().scale();
      }

      glm::vec3 mix_color = ToVec3(appearance.mix_color());
      data.color = glm::vec4(mix_color, appearance.mix_percent());

      draw_data->texture_walls.push_back(data);
      return;
    }
    AddDrawWallSolidColor(transform, GetSolidColor(appearance), is_cylinder_wall, draw_data);
  }

  void AddDrawWallSolidColor(const glm::mat4& transform,
                             const glm::vec3& color,
                             bool is_cylinder_wall,
                             DrawData* draw_data) {
    SolidColorInstanceData data;
    data.transform = transform;
    data.color = glm::vec4(color, 1.0f);
    if (is_cylinder_wall) {
      draw_data->solid_cylinder_walls.push_back(data);
    } else {
      draw_data->solid_quads.push_back(data);
    }
  }

  void AddDrawTargets(const glm::mat4& view_projection,
                      const LookAtInfo& look_at,
                      const Theme& theme,
                      const HealthBarSettings& health_bar_settings,
                      const std::vector<Target>& targets,
                      bool try_to_draw_center,
                      DrawData* draw_data) {
    // Only try to draw center if the theme has a specific color specified.
    try_to_draw_center = try_to_draw_center && theme.has_center_target_color();

    glm::vec3 target_color = theme.has_target_color() ? ToVec3(theme.target_color()) : glm::vec3(0);
    glm::vec3 center_target_color =
        theme.has_center_target_color() ? ToVec3(theme.center_target_color()) : target_color;
    glm::vec3 ghost_target_color =
        theme.has_ghost_target_color() ? ToVec3(theme.ghost_target_color()) : glm::vec3(0.3);

    auto& h = theme.health_bar();
    auto left = ToVec3(h.health_color());
    auto right = ToVec3(h.background_color());
    glm::vec4 health_color(left.r, left.g, left.b, h.has_health_alpha() ? h.health_alpha() : 1.0f);
    glm::vec4 health_background_color(
        right.r, right.g, right.b, h.has_background_alpha() ? h.background_alpha() : 1.0f);

    bool should_draw = false;
    for (const Target& target : targets) {
      if (target.ShouldDraw()) {
        should_draw = true;
      }
    }
    if (!should_draw) {
      return;
    }

    for (const Target& target : targets) {
      if (!target.ShouldDraw()) {
        continue;
      }
      const glm::vec3& color = target.is_ghost ? ghost_target_color : target_color;
      bool should_draw_center = try_to_draw_center && !target.is_ghost;
      if (target.is_pill) {
        Cylinder c;
        c.radius = target.radius;
        c.up = target.pill_up;
        c.height = target.height - target.radius;
        c.position = target.position;

        AddDrawCylinderQuad(view_projection, c, color, look_at, draw_data);

        glm::vec3 top = c.position + c.up * (c.height * 0.5f);
        AddDrawSphereCircle(view_projection, top, target.radius, color, look_at, draw_data);

        glm::vec3 bottom = c.position + c.up * (c.height * -0.5f);
        AddDrawSphereCircle(view_projection, bottom, target.radius, color, look_at, draw_data);

        continue;
      }

      if (should_draw_center) {
        AddDrawSphereCircle(
            view_projection, target.position, target.radius, color, look_at, draw_data);
        AddDrawSphere(view_projection,
                      target.position,
                      target.radius * kCenterSphereSizeMultiplier,
                      center_target_color,
                      draw_data);
      } else {
        // AddDrawSphere(view_projection, target.position, target.radius, color, draw_data);
        AddDrawSphereCircle(
            view_projection, target.position, target.radius, color, look_at, draw_data);
      }

      if (health_bar_settings.show() && target.HasHealth()) {
        bool is_damaged = target.GetHealthPercent() < 1;
        if (!health_bar_settings.only_damaged() || is_damaged) {
          AddDrawHealthBar(view_projection,
                           target.GetHealthPercent(),
                           target.position,
                           target.radius,
                           look_at,
                           health_bar_settings,
                           health_color,
                           health_background_color,
                           draw_data);
        }
      }
    }
  }

  void AddDrawHealthBar(const glm::mat4& view_projection,
                        float health_percent,
                        const glm::vec3& position,
                        float radius,
                        const LookAtInfo& look_at,
                        const HealthBarSettings& health_bar_settings,
                        const glm::vec4& health_color,
                        const glm::vec4& health_background_color,
                        DrawData* draw_data) {
    float width = radius * 2.8;
    if (health_bar_settings.has_size()) {
      width *= health_bar_settings.size();
    }

    float height = width * 0.18;

    float height_above_target = height * 0.35;
    glm::vec3 up = glm::vec3(0, 0, 1);
    glm::vec3 health_bar_center = position + up * (height_above_target + radius + (height / 2.0f));

    glm::mat4 transform(1.0f);
    transform = glm::translate(transform, health_bar_center);

    transform = RotateBasicShapeTowardsCamera(look_at, position, transform, /*rotate_z=*/false);

    transform = glm::scale(transform, glm::vec3(width, 1, height));

    if (health_percent >= 1) {
      // All health
      draw_data->solid_quads.emplace_back();
      SolidColorInstanceData& data = draw_data->solid_quads.back();
      data.transform = view_projection * transform;
      data.color = health_color;
      return;
    }

    if (health_percent <= 0) {
      // All background
      draw_data->solid_quads.emplace_back();
      SolidColorInstanceData& data = draw_data->solid_quads.back();
      data.transform = view_projection * transform;
      data.color = health_background_color;
      return;
    }

    {
      glm::mat4 health_transform =
          glm::translate(transform, glm::vec3((health_percent - 1) * 0.5f, 0.0f, 0.0f));
      health_transform = glm::scale(health_transform, glm::vec3(health_percent, 1, 1));
      draw_data->solid_quads.emplace_back();
      SolidColorInstanceData& data = draw_data->solid_quads.back();
      data.transform = view_projection * health_transform;
      data.color = health_color;
    }

    {
      glm::mat4 background_transform =
          glm::translate(transform, glm::vec3(health_percent * 0.5f, 0.0f, 0.0f));
      background_transform =
          glm::scale(background_transform, glm::vec3((1 - health_percent), 1, 1));
      draw_data->solid_quads.emplace_back();
      SolidColorInstanceData& data = draw_data->solid_quads.back();
      data.transform = view_projection * background_transform;
      data.color = health_background_color;
    }
  }

  void AddDrawSphere(const glm::mat4& view_projection,
                     const glm::vec3& position,
                     float radius,
                     const glm::vec3& color,
                     DrawData* draw_data) {
    draw_data->solid_spheres.emplace_back();
    SolidColorInstanceData& data = draw_data->solid_spheres.back();
    glm::mat4 transform(1.0f);
    transform = glm::translate(transform, position);
    transform = glm::scale(transform, glm::vec3(radius));
    data.transform = view_projection * transform;
    data.color = glm::vec4(color, 1.0f);
  }

  void AddDrawSphereCircle(const glm::mat4& view_projection,
                           const glm::vec3& position,
                           float radius,
                           const glm::vec3& color,
                           const LookAtInfo& look_at,
                           DrawData* draw_data) {
    draw_data->solid_circles.emplace_back();
    SolidColorInstanceData& data = draw_data->solid_circles.back();

    glm::mat4 model(1.0f);
    model = glm::translate(model, position);
    model = RotateBasicShapeTowardsCamera(look_at, position, model, /*rotate_z=*/true);
    model = glm::scale(model, glm::vec3(radius, 1.0, radius));

    data.transform = view_projection * model;
    data.color = glm::vec4(color, 1.0f);
  }

  void AddDrawCylinder(const glm::mat4& view_projection,
                       const Cylinder& c,
                       const glm::vec3& color,
                       DrawData* draw_data) {
    draw_data->solid_cylinders.emplace_back();
    SolidColorInstanceData& data = draw_data->solid_cylinders.back();

    glm::mat4 model(1.0f);
    model = glm::translate(model, c.position);
    if (c.up != glm::vec3(0, 0, 1) && c.up != glm::vec3(0, 0, -1)) {
      glm::vec3 up = glm::vec3(0, 0, 1);
      glm::vec3 rotate_axis = glm::normalize(glm::cross(up, c.up));
      float angle = glm::acos(glm::dot(up, c.up));
      model = glm::rotate(model, angle, rotate_axis);
    }
    model = glm::scale(model, glm::vec3(c.radius, c.radius, c.height));

    data.transform = view_projection * model;
    data.color = glm::vec4(color, 1.0f);
  }

  void AddDrawCylinderQuad(const glm::mat4& view_projection,
                           const Cylinder& c,
                           const glm::vec3& color,
                           const LookAtInfo& look_at,
                           DrawData* draw_data) {
    draw_data->solid_quads.emplace_back();
    SolidColorInstanceData& data = draw_data->solid_quads.back();

    glm::mat4 model(1.0f);
    model = glm::translate(model, c.position);
    if (c.up != glm::vec3(0, 0, 1) && c.up != glm::vec3(0, 0, -1)) {
      glm::vec3 up = glm::vec3(0, 0, 1);
      glm::vec3 rotate_axis = glm::normalize(glm::cross(up, c.up));
      float angle = glm::acos(glm::dot(up, c.up));
      model = glm::rotate(model, angle, rotate_axis);
    }
    model = RotateBasicShapeTowardsCamera(look_at, c.position, model, /*rotate_z=*/false);
    model = glm::scale(model, glm::vec3(c.radius * 2, 1.0, c.height));

    data.transform = view_projection * model;
    data.color = glm::vec4(color, 1.0f);
  }

  TextureManager* texture_manager_;
};

class RendererImpl : public Renderer {
 public:
  RendererImpl(const std::vector<std::filesystem::path>& texture_dirs,
               SDL_GPUSampleCount msaa_sample_count,
               SDL_GPUDevice* device,
               SDL_Window* sdl_window)
      : device_(device),
        sdl_window_(sdl_window),
        msaa_sample_count_(msaa_sample_count),
        texture_manager_(texture_dirs, device),
        draw_data_builder_(&texture_manager_) {
    SDL_GetWindowSizeInPixels(sdl_window_, &viewport_width_, &viewport_height_);
  }

  ~RendererImpl() override {
    Cleanup();
  }

  void Cleanup() override {
    CleanupShaders();
    if (solid_color_pipeline_ != nullptr) {
      SDL_ReleaseGPUGraphicsPipeline(device_, solid_color_pipeline_);
      solid_color_pipeline_ = nullptr;
    }
    if (texture_quad_pipeline_ != nullptr) {
      SDL_ReleaseGPUGraphicsPipeline(device_, texture_quad_pipeline_);
      texture_quad_pipeline_ = nullptr;
    }
    if (packed_vertex_buffer_ != nullptr) {
      SDL_ReleaseGPUBuffer(device_, packed_vertex_buffer_);
      packed_vertex_buffer_ = nullptr;
    }
    if (packed_textured_vertex_buffer_ != nullptr) {
      SDL_ReleaseGPUBuffer(device_, packed_textured_vertex_buffer_);
      packed_textured_vertex_buffer_ = nullptr;
    }
    if (solid_color_instance_buffer_ != nullptr) {
      SDL_ReleaseGPUBuffer(device_, solid_color_instance_buffer_);
      solid_color_instance_buffer_ = nullptr;
    }
    if (solid_color_instance_transfer_buffer_ != nullptr) {
      SDL_ReleaseGPUTransferBuffer(device_, solid_color_instance_transfer_buffer_);
      solid_color_instance_transfer_buffer_ = nullptr;
    }
    if (depth_texture_ != nullptr) {
      SDL_ReleaseGPUTexture(device_, depth_texture_);
      depth_texture_ = nullptr;
    }
    if (msaa_render_texture_ != nullptr) {
      SDL_ReleaseGPUTexture(device_, msaa_render_texture_);
      msaa_render_texture_ = nullptr;
    }
    texture_manager_.clear();
  }

  void CleanupShaders() {
    if (solid_color_instanced_vertex_shader_ != nullptr) {
      SDL_ReleaseGPUShader(device_, solid_color_instanced_vertex_shader_);
      solid_color_instanced_vertex_shader_ = nullptr;
    }
    if (solid_color_instanced_fragment_shader_ != nullptr) {
      SDL_ReleaseGPUShader(device_, solid_color_instanced_fragment_shader_);
      solid_color_instanced_fragment_shader_ = nullptr;
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
    solid_color_instanced_vertex_shader_ = LoadShader(
        device_, shader_dir, "solid_color_instanced.vert", SDL_GPU_SHADERSTAGE_VERTEX, 1, 0, 1);
    solid_color_instanced_fragment_shader_ =
        LoadShader(device_, shader_dir, "solid_color_instanced.frag", SDL_GPU_SHADERSTAGE_FRAGMENT);
    texture_fragment_shader_ =
        LoadShader(device_, shader_dir, "texture.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1);
    position_and_tex_coord_vertex_shader_ = LoadShader(
        device_, shader_dir, "position_and_texture_coord.vert", SDL_GPU_SHADERSTAGE_VERTEX, 1);

    SDL_GPUTextureCreateInfo depth_texture_info{};
    depth_texture_info.type = SDL_GPU_TEXTURETYPE_2D;
    depth_texture_info.width = viewport_width_;
    depth_texture_info.height = viewport_height_;

    depth_texture_info.layer_count_or_depth = 1;
    depth_texture_info.num_levels = 1;
    depth_texture_info.sample_count = msaa_sample_count_;
    depth_texture_info.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    depth_texture_info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    depth_texture_ = SDL_CreateGPUTexture(device_, &depth_texture_info);

    if (msaa_sample_count_ != SDL_GPU_SAMPLECOUNT_1) {
      SDL_GPUTextureCreateInfo render_texture_info{};
      render_texture_info.type = SDL_GPU_TEXTURETYPE_2D;
      render_texture_info.width = viewport_width_;
      render_texture_info.height = viewport_height_;
      render_texture_info.layer_count_or_depth = 1;
      render_texture_info.num_levels = 1;
      render_texture_info.format = SDL_GetGPUSwapchainTextureFormat(device_, sdl_window_);
      render_texture_info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
      render_texture_info.sample_count = msaa_sample_count_;
      msaa_render_texture_ = SDL_CreateGPUTexture(device_, &render_texture_info);
    }

    if (!CreateTextureQuadPipeline()) {
      return false;
    }
    if (!CreateSolidColorPipeline()) {
      return false;
    }

    SDL_GPUCommandBuffer* upload_command_buffer = SDL_AcquireGPUCommandBuffer(device_);
    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(upload_command_buffer);

    std::vector<VertexAndTexCoord> cylinder_wall_vertices = GenerateCylinderWallVertices(400);
    num_cylinder_wall_vertices_ = cylinder_wall_vertices.size();

    std::vector<VertexAndTexCoord> quad_vertices = GenerateQuadVertices();
    assert(quad_vertices.size() == kQuadNumVertices && "kQuadNumVertices is not correct");

    SDL_GPUTransferBuffer* packed_transfer_buffer =
        CreatePackedGeometryVertexBuffer(quad_vertices, cylinder_wall_vertices, copy_pass);
    SDL_GPUTransferBuffer* packed_textured_transfer_buffer =
        CreatePackedTexturedGeometryVertexBuffer(quad_vertices, cylinder_wall_vertices, copy_pass);

    {
      u32 instance_buffer_size = sizeof(SolidColorInstanceData) * kMaxSolidColorInstances;
      SDL_GPUBufferCreateInfo buffer_info{};
      buffer_info.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
      buffer_info.size = instance_buffer_size;
      solid_color_instance_buffer_ = SDL_CreateGPUBuffer(device_, &buffer_info);

      SDL_GPUTransferBufferCreateInfo transfer_info{};
      transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
      transfer_info.size = instance_buffer_size;
      solid_color_instance_transfer_buffer_ = SDL_CreateGPUTransferBuffer(device_, &transfer_info);
    }

    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(upload_command_buffer);
    SDL_ReleaseGPUTransferBuffer(device_, packed_transfer_buffer);
    SDL_ReleaseGPUTransferBuffer(device_, packed_textured_transfer_buffer);

    CleanupShaders();
    return true;
  }

  void DrawScenario(const glm::mat4& projection,
                    const Room& room,
                    ShotType::TypeCase shot_type,
                    const Theme& theme,
                    const HealthBarSettings& health_bar,
                    const std::vector<Target>& targets,
                    const LookAtInfo& look_at,
                    RenderContext* ctx) {
    const Stopwatch& stopwatch = *ctx->stopwatch;
    FrameTimes* times = ctx->times;
    const glm::mat4 view_projection = projection * look_at.transform;

    times->build_draw_data.start = stopwatch.GetElapsedMicros();
    DrawData draw_data;
    draw_data_builder_.GetDrawDataForScenario(view_projection,
                                              room,
                                              shot_type == ShotType::kTrackingProximity,
                                              theme,
                                              health_bar,
                                              targets,
                                              look_at,
                                              &draw_data);
    times->build_draw_data.end = stopwatch.GetElapsedMicros();

    SolidColorInstances solid_color_instances;
    UploadSolidColorInstanceData(draw_data, &solid_color_instances, times, stopwatch, ctx);

    // Setup and start a render pass
    SDL_GPUColorTargetInfo target_info = {};
    target_info.clear_color = SDL_FColor{0, 0, 0, 1.0};
    target_info.load_op = SDL_GPU_LOADOP_CLEAR;
    target_info.texture =
        msaa_sample_count_ == SDL_GPU_SAMPLECOUNT_1 ? ctx->swapchain_texture : msaa_render_texture_;
    target_info.store_op = SDL_GPU_STOREOP_STORE;
    target_info.mip_level = 0;
    target_info.layer_or_depth_plane = 0;
    target_info.cycle = true;

    SDL_GPUDepthStencilTargetInfo depth_stencil_target_info = {0};
    depth_stencil_target_info.texture = depth_texture_;
    depth_stencil_target_info.cycle = true;
    depth_stencil_target_info.clear_depth = 1;
    depth_stencil_target_info.clear_stencil = 0;
    depth_stencil_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
    depth_stencil_target_info.store_op = SDL_GPU_STOREOP_STORE;
    depth_stencil_target_info.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
    depth_stencil_target_info.stencil_store_op = SDL_GPU_STOREOP_STORE;

    times->begin_render_pass = stopwatch.GetElapsedMicros();
    ctx->render_pass =
        SDL_BeginGPURenderPass(ctx->command_buffer, &target_info, 1, &depth_stencil_target_info);

    times->render_draw_data.start = stopwatch.GetElapsedMicros();
    RenderDrawData(draw_data, solid_color_instances, ctx);
    times->render_draw_data.end = stopwatch.GetElapsedMicros();

    times->end_render_pass = stopwatch.GetElapsedMicros();
    SDL_EndGPURenderPass(ctx->render_pass);
    ctx->render_pass = nullptr;
  }

  void RenderImGui(std::optional<ImVec4> explicit_clear_color) override {
    ImVec4 clear_color;
    if (explicit_clear_color) {
      clear_color = *explicit_clear_color;
    } else {
      ImGuiStyle& style = ImGui::GetStyle();
      clear_color = style.Colors[ImGuiCol_WindowBg];
    }
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    const bool is_minimized =
        (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);

    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device_);

    SDL_GPUTexture* swapchain_texture;
    SDL_WaitAndAcquireGPUSwapchainTexture(command_buffer,
                                          sdl_window_,
                                          &swapchain_texture,
                                          nullptr,
                                          nullptr);  // Acquire a swapchain texture

    if (swapchain_texture != nullptr && !is_minimized) {
      // This is mandatory: call ImGui_ImplSDLGPU3_PrepareDrawData() to upload the vertex/index
      // buffer!
      ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, command_buffer);

      // Setup and start a render pass
      SDL_GPUColorTargetInfo target_info = {};
      target_info.clear_color =
          SDL_FColor{clear_color.x, clear_color.y, clear_color.z, clear_color.w};
      target_info.load_op = SDL_GPU_LOADOP_CLEAR;
      if (msaa_sample_count_ == SDL_GPU_SAMPLECOUNT_1) {
        target_info.texture = swapchain_texture;
        target_info.store_op = SDL_GPU_STOREOP_STORE;
      } else {
        target_info.texture = msaa_render_texture_;
        target_info.resolve_texture = swapchain_texture;
        target_info.store_op = SDL_GPU_STOREOP_RESOLVE;
      }
      target_info.mip_level = 0;
      target_info.layer_or_depth_plane = 0;
      target_info.cycle = false;
      SDL_GPURenderPass* render_pass =
          SDL_BeginGPURenderPass(command_buffer, &target_info, 1, nullptr);

      // Render ImGui
      ImGui_ImplSDLGPU3_RenderDrawData(draw_data, command_buffer, render_pass);

      SDL_EndGPURenderPass(render_pass);
    }

    // Submit the command buffer
    SDL_SubmitGPUCommandBuffer(command_buffer);
  }

  void RenderScenario(const glm::mat4& projection,
                      const Room& room,
                      ShotType::TypeCase shot_type,
                      const Theme& theme,
                      const HealthBarSettings& health_bar,
                      const std::vector<Target>& targets,
                      const LookAtInfo& look_at,
                      RenderContext* ctx) override {
    RenderContext ctx_local;
    if (ctx == nullptr) {
      ctx = &ctx_local;
    }

    if (StartRender(ctx)) {
      DrawScenario(projection, room, shot_type, theme, health_bar, targets, look_at, ctx);
      FinishRender(ctx);
    }
  }

  bool StartRender(RenderContext* ctx) {
    ctx->times->start_render.start = ctx->stopwatch->GetElapsedMicros();
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    const bool is_minimized =
        (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
    ctx->command_buffer = SDL_AcquireGPUCommandBuffer(device_);
    ctx->times->acquire_swapchain = ctx->stopwatch->GetElapsedMicros();
    SDL_AcquireGPUSwapchainTexture(ctx->command_buffer,
                                   sdl_window_,
                                   &ctx->swapchain_texture,
                                   nullptr,
                                   nullptr);  // Acquire a swapchain texture

    if (ctx->swapchain_texture == nullptr) {
      ctx->times->submit_swapchain_command_buffer = ctx->stopwatch->GetElapsedMicros();
      SDL_SubmitGPUCommandBuffer(ctx->command_buffer);
      ctx->times->start_render.end = ctx->stopwatch->GetElapsedMicros();
      return false;
    }

    ctx->times->imgui_prepare_draw_data = ctx->stopwatch->GetElapsedMicros();
    ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, ctx->command_buffer);
    ctx->times->start_render.end = ctx->stopwatch->GetElapsedMicros();
    return true;
  }

  void FinishRender(RenderContext* ctx) {
    ctx->times->finish_render.start = ctx->stopwatch->GetElapsedMicros();
    // Setup and start a render pass
    SDL_GPUColorTargetInfo target_info = {};
    target_info.load_op = SDL_GPU_LOADOP_LOAD;
    if (msaa_sample_count_ == SDL_GPU_SAMPLECOUNT_1) {
      target_info.texture = ctx->swapchain_texture;
      target_info.store_op = SDL_GPU_STOREOP_STORE;
    } else {
      target_info.texture = msaa_render_texture_;
      target_info.resolve_texture = ctx->swapchain_texture;
      target_info.store_op = SDL_GPU_STOREOP_RESOLVE;
    }
    target_info.mip_level = 0;
    target_info.layer_or_depth_plane = 0;
    target_info.cycle = false;

    SDL_PushGPUDebugGroup(ctx->command_buffer, "Render ImGui");

    ctx->times->imgui_begin_render_pass = ctx->stopwatch->GetElapsedMicros();
    auto* imgui_render_pass = SDL_BeginGPURenderPass(ctx->command_buffer, &target_info, 1, nullptr);

    ctx->times->imgui_render_draw_data = ctx->stopwatch->GetElapsedMicros();
    ImDrawData* draw_data = ImGui::GetDrawData();
    ImGui_ImplSDLGPU3_RenderDrawData(draw_data, ctx->command_buffer, imgui_render_pass);

    ctx->times->imgui_end_render_pass = ctx->stopwatch->GetElapsedMicros();
    SDL_EndGPURenderPass(imgui_render_pass);
    SDL_PopGPUDebugGroup(ctx->command_buffer);

    ctx->times->finish_render_submit_command_buffer = ctx->stopwatch->GetElapsedMicros();
    SDL_SubmitGPUCommandBuffer(ctx->command_buffer);

    ctx->times->finish_render.end = ctx->stopwatch->GetElapsedMicros();
  }

 private:
  void RenderDrawData(const DrawData& draw_data,
                      const SolidColorInstances& solid_color_instances,
                      RenderContext* ctx) {
    SDL_PushGPUDebugGroup(ctx->command_buffer, "Render Scene");
    RenderTextureWallsDrawData(draw_data, ctx);
    RenderSolidColorDrawData(draw_data, solid_color_instances, ctx);
    SDL_PopGPUDebugGroup(ctx->command_buffer);
  }

  void RenderTextureWallsDrawData(const DrawData& draw_data, RenderContext* ctx) {
    if (draw_data.texture_walls.empty()) {
      return;
    }
    SDL_PushGPUDebugGroup(ctx->command_buffer, "Render TextureWalls");
    Texture* last_bound_texture = nullptr;

    SDL_BindGPUGraphicsPipeline(ctx->render_pass, texture_quad_pipeline_);

    SDL_GPUBufferBinding binding{};
    binding.buffer = packed_textured_vertex_buffer_;
    binding.offset = 0;
    SDL_BindGPUVertexBuffers(ctx->render_pass, 0, &binding, 1);

    for (const TextureWallDrawData& data : draw_data.texture_walls) {
      assert(data.texture != nullptr && "Trying to render wall with no texture");
      TexScaleAndTransform tex_scale_and_transform{};
      tex_scale_and_transform.tex_scale.x = data.tex_scale.x;
      tex_scale_and_transform.tex_scale.y = data.tex_scale.y;
      tex_scale_and_transform.transform = data.transform;

      SDL_PushGPUVertexUniformData(ctx->command_buffer,
                                   0,
                                   &tex_scale_and_transform.tex_scale[0],
                                   sizeof(TexScaleAndTransform));

      SDL_PushGPUFragmentUniformData(ctx->command_buffer, 0, &data.color[0], sizeof(glm::vec4));
      if (data.texture != last_bound_texture) {
        last_bound_texture = data.texture;
        SDL_BindGPUFragmentSamplers(
            ctx->render_pass, 0, data.texture->texture_sampler_binding(), 1);
      }

      int num_vertices = kQuadNumVertices;
      int offset = textured_quad_vertices_offset_;
      if (data.is_cylinder) {
        num_vertices = num_cylinder_wall_vertices_;
        offset = textured_cylinder_wall_vertices_offset_;
      }
      SDL_DrawGPUPrimitives(ctx->render_pass, num_vertices, 1, offset, 0);
    }

    SDL_PopGPUDebugGroup(ctx->command_buffer);
  }

  void AddSolidColorInstancesOfType(std::vector<SolidColorInstanceData>& dest,
                                    const std::vector<SolidColorInstanceData>& values,
                                    u32* num_of_type) {
    i64 start_size = dest.size();
    i64 new_potential_max = dest.size() + values.size();
    i64 num_over_max = new_potential_max - kMaxSolidColorInstances;
    if (num_over_max <= 0) {
      dest.insert(dest.end(), values.begin(), values.end());
    } else {
      i64 num_to_include = values.size() - num_over_max;
      if (num_to_include > 0) {
        dest.insert(dest.end(), values.begin(), values.begin() + num_to_include);
      }
    }
    i64 end_size = dest.size();
    *num_of_type = end_size - start_size;
  };

  void UploadSolidColorInstanceData(const DrawData& draw_data,
                                    SolidColorInstances* instances,
                                    FrameTimes* times,
                                    const Stopwatch& stopwatch,
                                    RenderContext* ctx) {
    times->pack_instance_data.start = stopwatch.GetElapsedMicros();
    instances->instances.reserve(draw_data.solid_spheres.size() + draw_data.solid_cylinders.size() +
                                 draw_data.solid_quads.size() +
                                 draw_data.solid_cylinder_walls.size() +
                                 draw_data.solid_circles.size());

    AddSolidColorInstancesOfType(
        instances->instances, draw_data.solid_spheres, &instances->num_spheres);
    AddSolidColorInstancesOfType(
        instances->instances, draw_data.solid_cylinders, &instances->num_cylinders);
    AddSolidColorInstancesOfType(
        instances->instances, draw_data.solid_quads, &instances->num_quads);
    AddSolidColorInstancesOfType(
        instances->instances, draw_data.solid_cylinder_walls, &instances->num_cylinder_walls);
    AddSolidColorInstancesOfType(
        instances->instances, draw_data.solid_circles, &instances->num_circles);

    times->pack_instance_data.end = stopwatch.GetElapsedMicros();

    // Make sure we fit in the preallocated buffer size and draw nothing worst case. The above loops
    // should ensure we don't add too many items to the vector.
    if (instances->instances.size() > kMaxSolidColorInstances) {
      assert(false && "Too many items in solid color instance buffer");
      return;
    }

    if (instances->instances.empty()) {
      return;
    }

    times->upload_instance_data.start = stopwatch.GetElapsedMicros();

    SDL_PushGPUDebugGroup(ctx->command_buffer, "Upload SolidColor");

    times->upload_instance_data_memcpy.start = stopwatch.GetElapsedMicros();
    u32 data_size = sizeof(SolidColorInstanceData) * instances->instances.size();
    void* mapped_ptr =
        SDL_MapGPUTransferBuffer(device_, solid_color_instance_transfer_buffer_, /*cycle=*/true);
    SDL_memcpy(mapped_ptr, instances->instances.data(), data_size);
    SDL_UnmapGPUTransferBuffer(device_, solid_color_instance_transfer_buffer_);
    times->upload_instance_data_memcpy.end = stopwatch.GetElapsedMicros();

    times->upload_instance_data_copy_pass.start = stopwatch.GetElapsedMicros();
    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(ctx->command_buffer);

    SDL_GPUTransferBufferLocation source = {
        .transfer_buffer = solid_color_instance_transfer_buffer_, .offset = 0};
    SDL_GPUBufferRegion destination = {
        .buffer = solid_color_instance_buffer_,
        .offset = 0,
        .size = data_size,
    };
    SDL_UploadToGPUBuffer(copy_pass, &source, &destination, false);

    SDL_EndGPUCopyPass(copy_pass);
    SDL_PopGPUDebugGroup(ctx->command_buffer);
    times->upload_instance_data.end = stopwatch.GetElapsedMicros();
    times->upload_instance_data_copy_pass.end = times->upload_instance_data.end;
  }

  void RenderSolidColorDrawData(const DrawData& draw_data,
                                const SolidColorInstances& instances,
                                RenderContext* ctx) {
    if (instances.instances.empty()) {
      return;
    }

    SDL_PushGPUDebugGroup(ctx->command_buffer, "Render SolidColor");

    SDL_BindGPUGraphicsPipeline(ctx->render_pass, solid_color_pipeline_);
    // Bind instance data.
    SDL_BindGPUVertexStorageBuffers(ctx->render_pass, 0, &solid_color_instance_buffer_, 1);
    SDL_GPUBufferBinding binding{};
    binding.buffer = packed_vertex_buffer_;
    binding.offset = 0;
    SDL_BindGPUVertexBuffers(ctx->render_pass, 0, &binding, 1);

    auto set_instance_offset_uniform = [ctx](u32 offset) {
      SolidColorInstancedUniform uniform_data;
      uniform_data.instance_offset = offset;
      SDL_PushGPUVertexUniformData(
          ctx->command_buffer, 0, &uniform_data, sizeof(SolidColorInstancedUniform));
    };

    if (instances.num_spheres > 0) {
      set_instance_offset_uniform(instances.GetSpheresOffset());
      SDL_DrawGPUPrimitives(ctx->render_pass,
                            num_sphere_vertices_,
                            instances.num_spheres,
                            sphere_vertices_offset_,
                            0);
    }

    if (instances.num_cylinders > 0) {
      set_instance_offset_uniform(instances.GetCylindersOffset());
      SDL_DrawGPUPrimitives(ctx->render_pass,
                            num_cylinder_vertices_,
                            instances.num_cylinders,
                            cylinder_vertices_offset_,
                            0);
    }

    if (instances.num_quads > 0) {
      set_instance_offset_uniform(instances.GetQuadsOffset());
      SDL_DrawGPUPrimitives(
          ctx->render_pass, kQuadNumVertices, instances.num_quads, quad_vertices_offset_, 0);
    }

    if (instances.num_cylinder_walls > 0) {
      set_instance_offset_uniform(instances.GetCylinderWallsOffset());
      SDL_DrawGPUPrimitives(ctx->render_pass,
                            num_cylinder_wall_vertices_,
                            instances.num_cylinder_walls,
                            cylinder_wall_vertices_offset_,
                            0);
    }
    if (instances.num_circles > 0) {
      set_instance_offset_uniform(instances.GetCirclesOffset());
      SDL_DrawGPUPrimitives(ctx->render_pass,
                            num_circle_vertices_,
                            instances.num_circles,
                            circle_vertices_offset_,
                            0);
    }

    SDL_PopGPUDebugGroup(ctx->command_buffer);
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

  bool CreateSolidColorPipeline() {
    SDL_GPUGraphicsPipelineCreateInfo pipeline_info = CreateDefaultPipelineInfo(
        solid_color_instanced_vertex_shader_, solid_color_instanced_fragment_shader_);

    SDL_GPUColorTargetDescription color_target_desc[1];
    color_target_desc[0].format = SDL_GetGPUSwapchainTextureFormat(device_, sdl_window_);
    color_target_desc[0].blend_state = DefaultBlendState();
    pipeline_info.target_info.color_target_descriptions = color_target_desc;

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

    solid_color_pipeline_ = SDL_CreateGPUGraphicsPipeline(device_, &pipeline_info);
    if (solid_color_pipeline_ == nullptr) {
      Logger::get()->error("ERROR: SolidColorPipeline SDL_CreateGPUGraphicsPipeline failed: {}",
                           SDL_GetError());
      return false;
    }

    return true;
  }

  bool CreateTextureQuadPipeline() {
    SDL_GPUGraphicsPipelineCreateInfo pipeline_info =
        CreateDefaultPipelineInfo(position_and_tex_coord_vertex_shader_, texture_fragment_shader_);

    SDL_GPUColorTargetDescription color_target_desc[1];
    color_target_desc[0].format = SDL_GetGPUSwapchainTextureFormat(device_, sdl_window_);
    color_target_desc[0].blend_state = DefaultBlendState();
    pipeline_info.target_info.color_target_descriptions = color_target_desc;

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

    texture_quad_pipeline_ = SDL_CreateGPUGraphicsPipeline(device_, &pipeline_info);
    if (texture_quad_pipeline_ == nullptr) {
      Logger::get()->error("ERROR: TextureQuadPipeline SDL_CreateGPUGraphicsPipeline failed: {}",
                           SDL_GetError());
      return false;
    }

    return true;
  }

  SDL_GPUGraphicsPipelineCreateInfo CreateDefaultPipelineInfo(SDL_GPUShader* vertex_shader,
                                                              SDL_GPUShader* fragment_shader,
                                                              bool use_depth = true) {
    SDL_GPUGraphicsPipelineTargetInfo target_info = {};
    target_info.num_color_targets = 1;
    target_info.has_depth_stencil_target = true;
    target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.target_info = target_info;
    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeline_info.vertex_shader = vertex_shader;
    pipeline_info.fragment_shader = fragment_shader;
    pipeline_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipeline_info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    pipeline_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    pipeline_info.multisample_state.sample_count = msaa_sample_count_;

    SDL_GPUDepthStencilState depth_stencil_state{};
    depth_stencil_state.enable_depth_test = true;
    depth_stencil_state.enable_depth_write = true;
    depth_stencil_state.enable_stencil_test = false;
    depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    depth_stencil_state.write_mask = 0xFF;
    pipeline_info.depth_stencil_state = depth_stencil_state;

    return pipeline_info;
  }

  SDL_GPUTransferBuffer* CreatePackedGeometryVertexBuffer(
      const std::vector<VertexAndTexCoord>& quad_vertices,
      const std::vector<VertexAndTexCoord>& cylinder_wall_vertices,
      SDL_GPUCopyPass* copy_pass) {
    std::vector<float> sphere_vertices = GenerateSphereVertices(3);
    num_sphere_vertices_ = sphere_vertices.size() / 3;

    std::vector<glm::vec3> cylinder_vertices = GenerateCylinderVertices(100);
    num_cylinder_vertices_ = cylinder_vertices.size();

    auto circle_vertices = GenerateCircleVertices(200);
    num_circle_vertices_ = circle_vertices.size();

    std::vector<float> packed_data = sphere_vertices;
    packed_data.reserve(sphere_vertices.size() + (quad_vertices.size() * 3) +
                        (circle_vertices.size() * 3) + (cylinder_vertices.size() * 3) +
                        (cylinder_wall_vertices.size() * 3));

    for (const VertexAndTexCoord& vt : quad_vertices) {
      packed_data.push_back(vt.vertex.x);
      packed_data.push_back(vt.vertex.y);
      packed_data.push_back(vt.vertex.z);
    }
    for (const glm::vec3& v : cylinder_vertices) {
      packed_data.push_back(v.x);
      packed_data.push_back(v.y);
      packed_data.push_back(v.z);
    }

    for (const VertexAndTexCoord& vt : cylinder_wall_vertices) {
      packed_data.push_back(vt.vertex.x);
      packed_data.push_back(vt.vertex.y);
      packed_data.push_back(vt.vertex.z);
    }
    for (const glm::vec3& v : circle_vertices) {
      packed_data.push_back(v.x);
      packed_data.push_back(v.y);
      packed_data.push_back(v.z);
    }

    sphere_vertices_offset_ = 0;
    quad_vertices_offset_ = num_sphere_vertices_;
    cylinder_vertices_offset_ = quad_vertices_offset_ + kQuadNumVertices;
    cylinder_wall_vertices_offset_ = cylinder_vertices_offset_ + num_cylinder_vertices_;
    circle_vertices_offset_ = cylinder_wall_vertices_offset_ + num_cylinder_wall_vertices_;

    int size = sizeof(float) * packed_data.size();
    return UploadBuffer(packed_data.data(), size, copy_pass, &packed_vertex_buffer_);
  }

  SDL_GPUTransferBuffer* CreatePackedTexturedGeometryVertexBuffer(
      const std::vector<VertexAndTexCoord>& quad_vertices,
      const std::vector<VertexAndTexCoord>& cylinder_wall_vertices,
      SDL_GPUCopyPass* copy_pass) {
    std::vector<VertexAndTexCoord> packed_data;
    packed_data.reserve(quad_vertices.size() + cylinder_wall_vertices.size());
    packed_data.insert(packed_data.end(), quad_vertices.begin(), quad_vertices.end());
    packed_data.insert(
        packed_data.end(), cylinder_wall_vertices.begin(), cylinder_wall_vertices.end());

    textured_quad_vertices_offset_ = 0;
    textured_cylinder_wall_vertices_offset_ = quad_vertices.size();

    int size = sizeof(VertexAndTexCoord) * packed_data.size();
    return UploadBuffer(packed_data.data(), size, copy_pass, &packed_textured_vertex_buffer_);
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
    SDL_UploadToGPUBuffer(copy_pass, &location, &region, /*cycle=*/false);

    *vertex_buffer_out = vertex_buffer;
    return transfer_buffer;
  }

  SDL_GPUShader* solid_color_instanced_fragment_shader_ = nullptr;
  SDL_GPUShader* solid_color_instanced_vertex_shader_ = nullptr;
  SDL_GPUShader* position_and_tex_coord_vertex_shader_ = nullptr;
  SDL_GPUShader* texture_fragment_shader_ = nullptr;
  SDL_GPUGraphicsPipeline* solid_color_pipeline_;
  SDL_GPUGraphicsPipeline* texture_quad_pipeline_;
  SDL_GPUDevice* device_ = nullptr;
  SDL_Window* sdl_window_ = nullptr;

  SDL_GPUBuffer* packed_vertex_buffer_ = nullptr;
  SDL_GPUBuffer* packed_textured_vertex_buffer_ = nullptr;

  SDL_GPUBuffer* solid_color_instance_buffer_ = nullptr;
  SDL_GPUTransferBuffer* solid_color_instance_transfer_buffer_ = nullptr;

  SDL_GPUTexture* depth_texture_ = nullptr;
  SDL_GPUTexture* msaa_render_texture_ = nullptr;

  SDL_GPUSampleCount msaa_sample_count_;

  unsigned int num_sphere_vertices_;
  unsigned int num_cylinder_wall_vertices_;
  unsigned int num_cylinder_vertices_;
  unsigned int num_circle_vertices_;

  unsigned int sphere_vertices_offset_;
  unsigned int cylinder_vertices_offset_;
  unsigned int cylinder_wall_vertices_offset_;
  unsigned int quad_vertices_offset_;
  unsigned int circle_vertices_offset_;

  unsigned int textured_cylinder_wall_vertices_offset_;
  unsigned int textured_quad_vertices_offset_;

  int viewport_width_ = 0;
  int viewport_height_ = 0;

  TextureManager texture_manager_;
  DrawDataBuilder draw_data_builder_;
};

}  // namespace

std::unique_ptr<Renderer> CreateRenderer(const std::vector<std::filesystem::path>& texture_dirs,
                                         const std::filesystem::path& shader_dir,
                                         SDL_GPUSampleCount msaa_sample_count,
                                         SDL_GPUDevice* device,
                                         SDL_Window* sdl_window) {
  auto renderer =
      std::make_unique<RendererImpl>(texture_dirs, msaa_sample_count, device, sdl_window);
  if (!renderer->Initialize(shader_dir)) {
    return {};
  }
  return std::move(renderer);
}

}  // namespace aim
