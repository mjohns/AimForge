#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "aim/common/simple_types.h"
#include "aim/common/util.h"
#include "aim/proto/scenario.pb.h"

namespace aim {

// Convert cm/360 to radians per dot reported by mouse at the given dpi.
float CmPer360ToRadiansPerDot(float cm_per_360, float dpi);

struct LookAtInfo {
  glm::vec3 front;
  glm::vec3 right;
  glm::vec3 up;
  glm::vec3 position;
  glm::mat4 transform;
};

struct PitchYaw {
  float pitch = 0;
  float yaw = 0;
};

glm::mat4 GetPerspectiveTransformation(const ScreenInfo& screen, float fov = 103.0f);

struct CameraParams {
  CameraParams() {}
  CameraParams(const Room& room) {
    pitch = room.start_pitch();
    yaw = room.start_yaw();
    position = ToVec3(room.camera_position());
    if (room.has_camera_up()) {
      up = ToVec3(room.camera_up());
    }
    if (room.has_camera_front()) {
      front = ToVec3(room.camera_front());
    }
  }

  glm::vec3 position{0, 0, 0};
  glm::vec3 up{0, 0, 1};
  glm::vec3 front{0, 1, 0};
  float pitch = 0;
  float yaw = 0;
};

class Camera {
 public:
  Camera();
  explicit Camera(const CameraParams& params);

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

  void SetPitchYawLookingAtPoint(const glm::vec3& look_at_pos);

  void Update(int xrel, int yrel, float radians_per_dot);
  void UpdatePitch(float pitch);
  void UpdateYaw(float yaw);
  void UpdatePitchYaw(const PitchYaw& pitch_yaw);

 private:
  glm::vec3 position_;
  glm::vec3 up_;
  glm::vec3 front_;
  glm::vec3 right_;
  float pitch_;
  float yaw_;
  bool is_default_orientation_;
};

}  // namespace aim