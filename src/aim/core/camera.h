#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "aim/common/simple_types.h"

namespace aim {

// Convert cm/360 to radians per dot reported by mouse at the given dpi.
float CmPer360ToRadiansPerDot(float cm_per_360, float dpi);

struct LookAtInfo {
  glm::vec3 front;
  glm::vec3 right;
  glm::vec3 up;
  glm::mat4 transform;
};

LookAtInfo GetLookAt(const glm::vec3& position, const glm::vec3& front);

glm::vec3 GetNormalizedRight(const glm::vec3& v);

glm::mat4 GetPerspectiveTransformation(const ScreenInfo& screen, float fov = 103.0f);

class Camera {
 public:
  Camera();
  Camera(float pitch, float yaw);
  Camera(float pitch, float yaw, glm::vec3 position);
  // Defaults to looking (0, 1, 0)
  Camera(glm::vec3 position);

  LookAtInfo GetLookAt();
  const glm::vec3& GetPosition() const {
    return position_;
  }

  float GetPitch() {
    return pitch_;
  }

  float GetYaw() {
    return yaw_;
  }

  void Update(int xrel, int yrel, float radians_per_dot);

  void UpdatePitch(float pitch) {
    pitch_ = pitch;
  }
  void UpdateYaw(float yaw) {
    yaw_ = yaw;
  }

 private:
  glm::vec3 position_;
  float pitch_;
  float yaw_;
};

}  // namespace aim