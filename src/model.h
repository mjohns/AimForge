#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <random>
#include <vector>

#include "application.h"
#include "camera.h"
#include "imgui.h"
#include "room.h"
#include "sound.h"

namespace aim {

struct Target {
  uint16_t id = 0;
  glm::vec3 position{};
  float radius = 1.0f;
  bool hidden = false;
};

class TargetManager {
 public:
  TargetManager() {}

  Target AddTarget(Target t);
  void RemoveTarget(uint16_t target_id);
  Target ReplaceTarget(uint16_t target_id_to_replace, Target new_target);

  const std::vector<Target>& GetTargets() {
    return _targets;
  }

 private:
  uint16_t _target_id_counter = 0;
  std::vector<Target> _targets;
};

class ScenarioDef {
 public:
  ScenarioDef() {}
  virtual ~ScenarioDef() {}

  virtual Camera GetInitialCamera() = 0;
  virtual std::vector<Target> GetInitialTargets() = 0;
  // Maybe OnHit is better?
  virtual Target GetNewTarget() = 0;
  virtual Room GetRoom() = 0;
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

  Room GetRoom() override {
    RoomParams p;
    p.wall_height = _params.height;
    p.wall_width = _params.width;
    return Room(p);
  }

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