#include "geometry.h"

// For glm intersect
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/intersect.hpp>
#include <glm/trigonometric.hpp>

namespace aim {

glm::vec2 GetRandomPositionOnWall(const Wall& wall, std::mt19937* random_generator) {
  auto dist = std::uniform_real_distribution<float>(-0.5f, 0.5f);
  return glm::vec2(dist(*random_generator) * wall.width, dist(*random_generator) * wall.height);
}

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
  if (radius <= 0) {
    return glm::vec2(0);
  }
  auto dist_degrees = std::uniform_real_distribution<float>(0, 360);
  float rotate_radians = glm::radians(dist_degrees(*random_generator));
  double x = radius * std::cos(rotate_radians);
  double y = radius * std::sin(rotate_radians);
  return glm::vec2(x, y);
}

glm::vec2 GetRandomPositionInCircle(float min_radius,
                                    float max_radius,
                                    std::mt19937* random_generator) {
  auto dist_radius = std::uniform_real_distribution<float>(min_radius, max_radius);
  return GetRandomPositionOnCircle(dist_radius(*random_generator), random_generator);
}

glm::vec2 GetRandomPositionInRectangle(float width, float height, std::mt19937* random_generator) {
  auto dist = std::uniform_real_distribution<float>(-0.5, 0.5);
  float px = dist(*random_generator);
  float py = dist(*random_generator);
  return glm::vec2(px * width, py * height);
}

glm::vec2 GetRandomPositionInRectangle(float width,
                                       float height,
                                       float inner_width,
                                       float inner_height,
                                       std::mt19937* random_generator) {
  if (inner_height <= 0 && inner_width <= 0) {
    return GetRandomPositionInRectangle(width, height, random_generator);
  }
  if (inner_height <= 0) {
    inner_height = height;
  }
  if (inner_width <= 0) {
    inner_width = width;
  }

  float top_width = width;
  float top_height = height - inner_height;

  float side_width = width - inner_width;
  float side_height = inner_height;

  float top_area = top_width * top_height;
  float side_area = side_width * side_height;
  float top_percent = top_area / (top_area + side_area);
  auto dist = std::uniform_real_distribution<float>(0, 1);
  float roll = dist(*random_generator);
  if (roll <= top_percent) {
    // Place in top.
    glm::vec2 p = GetRandomPositionInRectangle(top_width, top_height, random_generator);
    if (p.y < 0) {
      p.y -= (inner_height * 0.5);
    } else {
      p.y += (inner_height * 0.5);
    }
    return p;
  } else {
    // Place on side.
    glm::vec2 p = GetRandomPositionInRectangle(side_width, side_height, random_generator);
    if (p.x < 0) {
      p.x -= (inner_width * 0.5);
    } else {
      p.x += (inner_width * 0.5);
    }
    return p;
  }
}

bool IsPointInEllipse(const glm::vec2& point, float x_radius, float y_radius) {
  if (x_radius <= 0 || y_radius <= 0) {
    return false;
  }
  double normalized_x = point.x / x_radius;
  double normalized_y = point.y / y_radius;

  return (normalized_x * normalized_x + normalized_y * normalized_y) <= 1.0;
}

bool IsPointInCircle(const glm::vec2& point, float radius) {
  return IsPointInEllipse(point, radius, radius);
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

glm::vec3 PointBetween(const glm::vec3& start, const glm::vec3& end, float percent_across) {
  return ((end - start) * percent_across) + start;
}

bool IntersectRayCylinder(const glm::vec3& mid_point,
                          const glm::vec3& cylinder_up,
                          float radius,
                          const glm::vec3& origin,
                          const glm::vec3& direction,
                          float* intersection_distance,
                          float* intersection_height) {
  glm::vec3 cylinder_front = glm::normalize(mid_point - origin);
  glm::vec3 cylinder_x = glm::normalize(glm::cross(cylinder_front, cylinder_up));

  glm::mat4 rotation = MakeCoordinateSystemTransform(cylinder_x, cylinder_front, cylinder_up);
  glm::mat4 transform(1.f);
  transform = glm::translate(transform, mid_point * -1.0f);
  transform = rotation * transform;

  glm::vec3 transformed_origin = transform * glm::vec4(origin, 1.0f);
  glm::vec3 transformed_direction = rotation * glm::vec4(direction, 1.0f);

  float plane_distance;
  bool has_plane_intersection = glm::intersectRayPlane(transformed_origin,
                                                       transformed_direction,
                                                       glm::vec3(0, 0, 0),
                                                       glm::vec3(0, -1, 0),
                                                       plane_distance);
  if (!has_plane_intersection) {
    return false;
  }

  glm::vec3 intersection_point = transformed_origin + transformed_direction * plane_distance;
  if (abs(intersection_point.x) > radius) {
    return false;
  }
  *intersection_height = intersection_point.z;
  // TODO: Calculate actual distance on the cylinder.
  *intersection_distance = plane_distance;
  return true;
}

bool IntersectRayPill(const Pill& pill,
                      const glm::vec3& origin,
                      const glm::vec3& direction,
                      float* intersection_distance) {
  float cylinder_intersection_distance;
  float cylinder_intersection_height;
  bool has_cylinder_hit = IntersectRayCylinder(pill.position,
                                               pill.up,
                                               pill.radius,
                                               origin,
                                               direction,
                                               &cylinder_intersection_distance,
                                               &cylinder_intersection_height);
  if (!has_cylinder_hit) {
    return false;
  }

  float full_pill_height = pill.height;
  float cylinder_height = (full_pill_height - pill.radius) * 0.5;

  if (abs(cylinder_intersection_height) <= cylinder_height) {
    // Intersecting with cylinder portion of pill.
    *intersection_distance = cylinder_intersection_distance;
    return true;
  }

  // Do a broad check to see if it could possible even pass through sphere portion.
  if (abs(cylinder_intersection_height) > (cylinder_height + pill.radius + 5)) {
    // Too far to hit sphere.
    return false;
  }

  float tip_offset = cylinder_intersection_height > 0 ? cylinder_height : -1 * cylinder_height;
  glm::vec3 sphere_position = pill.position + pill.up * tip_offset;

  return IntersectRaySphere(sphere_position, pill.radius, origin, direction, intersection_distance);
}

bool IntersectRaySphere(const glm::vec3& position,
                        float radius,
                        const glm::vec3& origin,
                        const glm::vec3& direction,
                        float* intersection_distance) {
  glm::vec3 intersection_point;
  glm::vec3 intersection_normal;
  bool is_hit = glm::intersectRaySphere(
      origin, direction, position, radius, intersection_point, intersection_normal);
  if (is_hit) {
    *intersection_distance = glm::length(intersection_point - origin);
  }
  return is_hit;
}

// Axes must be normalized.
glm::mat4 MakeCoordinateSystemTransform(const glm::vec3& x_axis,
                                        const glm::vec3& y_axis,
                                        const glm::vec3& z_axis) {
  // clang-format off
  glm::mat4 rotation = glm::mat4(
      x_axis.x, y_axis.x, z_axis.x, 0.0f,
      x_axis.y, y_axis.y, z_axis.y, 0.0f,
      x_axis.z, y_axis.z, z_axis.z, 0.0f,
      0.0f,     0.0f,     0.0f,     1.0f
  );
  // clang-format on
  return rotation;
}

std::optional<float> GetNormalizedMissedShotDistance(const glm::vec3& camera_position,
                                                     const glm::vec3& look_at,
                                                     const glm::vec3& position) {
  glm::vec3 direction_to_point = glm::normalize(position - camera_position);
  // Find distance on a normalized plane 1 unit in front of the camera position.
  glm::vec3 plane_origin = camera_position + look_at;
  float plane_distance;
  bool has_plane_intersection = glm::intersectRayPlane(
      camera_position, direction_to_point, plane_origin, look_at, plane_distance);
  if (!has_plane_intersection) {
    return {};
  }
  glm::vec3 intersection_point = camera_position + direction_to_point * plane_distance;
  return glm::length(plane_origin - intersection_point);
}

}  // namespace aim
