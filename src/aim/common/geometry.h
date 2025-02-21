#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <random>

#include "aim/common/simple_types.h"

namespace aim {

glm::vec2 GetRandomPositionInEllipse(float radius_x,
                                     float radius_y,
                                     std::mt19937* random_generator);

glm::vec2 GetRandomPositionInCircle(float min_radius,
                                    float max_radius,
                                    std::mt19937* random_generator);

glm::vec2 GetRandomPositionOnCircle(float radius, std::mt19937* random_generator);

bool IsPointInEllipse(const glm::vec2& point, float x_radius, float y_radius);
bool IsPointInCircle(const glm::vec2& point, float radius);

bool IsPointInRectangle(const glm::vec2& point, const glm::vec2& bottom_left, glm::vec2& top_right);

glm::vec2 RotateRadians(const glm::vec2& v, float radians);

glm::vec2 RotateDegrees(const glm::vec2& v, float degrees);

glm::vec3 PointBetween(const glm::vec3& start, const glm::vec3& end, float percent_across = 0.5);

// If intersects returns true and fills in the distance and the y value where it intersected. Y is
// the distance from the midpoint (at maximum being height/2).
bool IntersectRayCylinder(const Cylinder& cylinder,
                          const glm::vec3& origin,
                          const glm::vec3& direction,
                          float* intersection_distance,
                          std::optional<float>* intersection_y);

// intersection_height is the distance from the mid_point along the up vector (possibly negative).
bool IntersectRayCylinder(const glm::vec3& mid_point,
                                  const glm::vec3& up,
                                  float radius,
                                  const glm::vec3& origin,
                                  const glm::vec3& direction,
                                  float* intersection_distance,
                                  float* intersection_height);

bool IntersectRayPill(const Pill& pill,
                      const glm::vec3& origin,
                      const glm::vec3& direction,
                      float* intersection_distance);

// Axes must be normalized.
glm::mat4 MakeCoordinateSystemTransform(const glm::vec3& x_axis,
                                        const glm::vec3& y_axis,
                                        const glm::vec3& z_axis);

}  // namespace aim
