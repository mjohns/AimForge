#include "camera.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>

namespace aim {
namespace {

constexpr float kMaxPitch = glm::radians(80.0f);
constexpr float kMinPitch = -1 * kMaxPitch;
constexpr float kMaxYaw = glm::radians(80.0f + 90.0f);
constexpr float kMinYaw = -1 * glm::radians(10.0f);

constexpr float k90DegreesInRadians = glm::radians(90.0f);

void FillInLookAt(const glm::vec3& position, LookAtInfo* info) {
  info->front = glm::normalize(info->front);
  info->right = GetNormalizedRight(info->front);
  info->up = glm::normalize(glm::cross(info->right, info->front));
  info->transform = glm::lookAt(position, position + info->front, info->up);
}

}  // namespace

float CmPer360ToRadiansPerDot(float cm_per_360, float dpi) {
  float dots_per_cm = dpi / 2.54f;  // 1 inch = 2.54 cm
  float dots_per_360 = cm_per_360 * dots_per_cm;
  // Convert to radians.
  return glm::two_pi<float>() / dots_per_360;
}

glm::mat4 GetPerspectiveTransformation(const ScreenInfo& screen, float fov) {
  return glm::perspective(
      glm::radians(fov), (float)screen.width / (float)screen.height, 0.1f, 2000.0f);
}

glm::vec3 GetNormalizedRight(const glm::vec3& v) {
  return glm::normalize(glm::cross(v, glm::vec3(0.0f, 0.0f, 1.0f)));
}

LookAtInfo Camera::GetLookAt() {
  // sin(0) = 0, cos(0) = 1, sin(90) = 1, cos(90) = 0
  LookAtInfo info;
  info.front.x = cos(pitch_) * sin(yaw_);
  info.front.y = cos(pitch_) * cos(yaw_);
  info.front.z = sin(pitch_);

  FillInLookAt(position_, &info);
  return info;
}

LookAtInfo GetLookAt(const glm::vec3& position, const glm::vec3& front) {
  LookAtInfo info;
  info.front = front;
  FillInLookAt(position, &info);
  return info;
}

void Camera::Update(int xrel, int yrel, float radians_per_dot) {
  yaw_ += radians_per_dot * xrel;
  pitch_ -= radians_per_dot * yrel;
  pitch_ = glm::clamp(pitch_, kMinPitch, kMaxPitch);
  // TODO: clamp yaw?
}

Camera::Camera(float pitch, float yaw, glm::vec3 position)
    : pitch_(pitch), yaw_(yaw), position_(position) {}

Camera::Camera(glm::vec3 position) : pitch_(0), yaw_(0), position_(position) {}

Camera::Camera(float pitch, float yaw) : pitch_(pitch), yaw_(yaw), position_(glm::vec3(0, 0, 0)) {}

}  // namespace aim