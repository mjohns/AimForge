#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <random>
#include <vector>

#include "application.h"
#include "camera.h"
#include "imgui.h"
#include "sound.h"

namespace aim {

struct Target {
  uint16_t id = 0;
  glm::vec3 position{};
  float radius = 1.0f;
  bool hidden = false;
};

struct Room {
  float wall_height = 0.0;
  float wall_width = 0.0;
  float wall_distance = 100.0;
  // Percentage of the wall_height up from the floor to place the camera.
  float camera_height_percent = 0.5;

  void Draw(ImDrawList* draw_list, const glm::mat4& transform, const ScreenInfo& screen);
};

class TargetManager {
 public:
  TargetManager() {}

  Target AddTarget(Target t);
  void RemoveTarget(uint16_t target_id);

  std::unordered_map<uint16_t, Target>& GetTargetMap() {
    return _target_map;
  }

 private:
  uint16_t _target_id_counter = 0;
  std::unordered_map<uint16_t, Target> _target_map;
};

class ScenarioDef {
 public:
  ScenarioDef() {}
  virtual ~ScenarioDef() {}

  virtual Camera GetInitialCamera() = 0;
  virtual std::vector<Target> GetInitialTargets() = 0;
  // Maybe OnHit is better?
  virtual Target GetNewTarget() = 0;
};

struct StaticWallParams {
  int num_targets = 1;
  float width;
  float height;
  float target_radius = 2;
};

class StaticWallScenarioDef : public ScenarioDef {
 public:
  StaticWallScenarioDef(StaticWallParams params);
  Camera GetInitialCamera() override;
  std::vector<Target> GetInitialTargets() override;
  Target GetNewTarget() override;

 private:
  StaticWallParams _params;
  std::mt19937 _random_generator;
  std::uniform_real_distribution<float> _distribution_x;
  std::uniform_real_distribution<float> _distribution_z;
};

struct Sounds {
  std::unique_ptr<Sound> shoot;
  std::unique_ptr<Sound> kill;
};

class Scenario {
 public:
  Scenario(ScenarioDef* def);
  void Run(Application* app);

 private:
  Camera _camera;
  TargetManager _target_manager;
  ScenarioDef* _def;
};

ImVec2 GetScreenPosition(const glm::vec3& target,
                         const glm::mat4& transform,
                         const ScreenInfo& screen);

}  // namespace aim