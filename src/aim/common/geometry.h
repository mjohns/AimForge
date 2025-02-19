#pragma once

#include <glm/vec2.hpp>
#include <random>

namespace aim {

glm::vec2 GetRandomPositionInEllipse(float radius_x,
                                     float radius_y,
                                     std::mt19937* random_generator);

glm::vec2 GetRandomPositionOnCircle(float radius, std::mt19937* random_generator);

bool IsPointInEllipse(const glm::vec2& point, float x_radius, float y_radius);

bool IsPointInRectangle(const glm::vec2& point, const glm::vec2& bottom_left, glm::vec2& top_right);

glm::vec2 RotateRadians(const glm::vec2& v, float radians);

glm::vec2 RotateDegrees(const glm::vec2& v, float degrees);

}  // namespace aim
