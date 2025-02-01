#pragma once

#include <imgui.h>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <random>
#include <vector>

#include "aim/core/application.h"
#include "aim/core/camera.h"
#include "aim/scenario/target.h"
#include "aim/graphics/room.h"
#include "aim/audio/sound.h"

namespace aim {

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

class Scenario {
 public:
  Scenario(ScenarioDef* def);
  void Run(Application* app);

 private:
  Camera _camera;
  TargetManager _target_manager;
  ScenarioDef* _def;
};

}  // namespace aim