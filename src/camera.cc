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

glm::mat4 Camera::GetLookAt() {
  glm::vec3 front;
  front.x = (float)(cos(_yaw) * cos(_pitch));
  front.y = (float)sin(_pitch);
  front.z = (float)(sin(_yaw) * cos(_pitch));
  front = glm::normalize(front);

  // Also re-calculate the Right and Up vector
  glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
  glm::vec3 up = glm::normalize(glm::cross(right, front));
  return glm::lookAt(glm::vec3(0, 0, 0), front, up);
}

glm::vec3 Camera::GetRight() {
  glm::vec3 front;
  front.x = (float)(cos(_yaw) * cos(_pitch));
  front.y = (float)sin(_pitch);
  front.z = (float)(sin(_yaw) * cos(_pitch));
  front = glm::normalize(front);

  return glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
}

void Camera::Update(int xrel, int yrel, float radians_per_pixel) {
  _yaw += radians_per_pixel * xrel;
  _pitch += radians_per_pixel * yrel;

  _pitch = glm::clamp(_pitch, kMinPitch, kMaxPitch);
  _yaw = glm::clamp(_yaw, kMinYaw, kMaxYaw);
}

Camera::Camera(float pitch, float yaw) : _pitch(pitch), _yaw(yaw) {
}

}  // namespace aim