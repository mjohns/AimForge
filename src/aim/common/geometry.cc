#include "geometry.h"

#include <glm/trigonometric.hpp>

namespace aim {

glm::vec2 GetRandomPositionInEllipse(float radius_x,
                                     float radius_y,
                                     std::mt19937* random_generator) {
  auto dist = std::uniform_real_distribution<float>(0, 1.0f);
  float r = sqrt(dist(*random_generator));
  auto dist_degrees = std::uniform_real_distribution<float>(0, 360);
  float rotate_radians = glm::radians(dist_degrees(*random_generator));

  double x = r * std::cos(rotate_radians);
  double y = r * std::sin(rotate_radians);

  return glm::vec2(x * radius_x, y * radius_y);
}

glm::vec2 GetRandomPositionOnCircle(float radius, std::mt19937* random_generator) {
  auto dist_degrees = std::uniform_real_distribution<float>(0, 360);
  float rotate_radians = glm::radians(dist_degrees(*random_generator));
  double x = radius * std::cos(rotate_radians);
  double y = radius * std::sin(rotate_radians);
  return glm::vec2(x, y);
}

bool IsPointInEllipse(const glm::vec2& point, float x_radius, float y_radius) {
  if (x_radius <= 0 || y_radius <= 0) {
    return false;
  }
  double normalized_x = point.x / x_radius;
  double normalized_y = point.y / y_radius;

  return (normalized_x * normalized_x + normalized_y * normalized_y) <= 1.0;
}

bool IsPointInRectangle(const glm::vec2& point,
                        const glm::vec2& bottom_left,
                        glm::vec2& top_right) {
  return point.x >= bottom_left.x && point.x <= top_right.x && point.y >= bottom_left.y &&
         point.y <= top_right.y;
}

glm::vec2 RotateRadians(const glm::vec2& v, float radians) {
  const float cos_theta = cos(radians);
  const float sin_theta = sin(radians);
  return glm::vec2(v.x * cos_theta - v.y * sin_theta, v.x * sin_theta + v.y * cos_theta);
}

glm::vec2 RotateDegrees(const glm::vec2& v, float degrees) {
  return RotateRadians(v, glm::radians<float>(degrees));
}

}  // namespace aim
