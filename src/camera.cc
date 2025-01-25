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

}  // namespace

// Convert cm/360 to radians per pixel
float CmPer360ToRadiansPerPixel(float cmPer360, float dpi) {
  // 1. Convert DPI (dots per inch) to dots per cm
  float dotsPerCm = dpi / 2.54f;  // 1 inch = 2.54 cm

  // 2. Calculate how many pixels for full 360 rotation
  float pixelsPer360 = cmPer360 * dotsPerCm;

  // 3. Convert to radians per pixel
  // 360 degrees = 2π radians
  float radiansPerPixel = glm::two_pi<float>() / pixelsPer360;

  return radiansPerPixel;
}

LookAtInfo Camera::GetLookAt() {
  // sin(0) = 0, cos(0) = 1, sin(90) = 1, cos(90) = 0
  // with pitch 0 = (x, 0, x)
  LookAtInfo info;
  info.front.x = cos(_pitch) * sin(_yaw);
  info.front.y = cos(_pitch) * cos(_yaw);
  info.front.z = sin(_pitch);
  info.front = glm::normalize(info.front);
  info.right = glm::normalize(glm::cross(info.front, glm::vec3(0.0f, 0.0f, 1.0f)));
  info.up = glm::normalize(glm::cross(info.right, info.front));

  info.transform = glm::lookAt(_position, _position + info.front, info.up);
  return info;
}

void Camera::Update(int xrel, int yrel, float radians_per_dot) {
  _yaw += radians_per_dot * xrel;
  _pitch -= radians_per_dot * yrel;
  _pitch = glm::clamp(_pitch, kMinPitch, kMaxPitch);
  // TODO: clamp yaw?

}

Camera::Camera(float pitch, float yaw, glm::vec3 position)
    : _pitch(pitch), _yaw(yaw), _position(position) {
}

Camera::Camera(glm::vec3 position) : _pitch(0), _yaw(0), _position(position) {
}

Camera::Camera(float pitch, float yaw) : _pitch(pitch), _yaw(yaw), _position(glm::vec3(0, 0, 0)) {
}

}  // namespace aim