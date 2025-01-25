#pragma once

#include "glm/vec3.hpp"
#include "glm/mat4x4.hpp"

namespace aim {

// Convert cm/360 to radians per pixel
float CmPer360ToRadiansPerPixel(float cm_per_360, float dpi);

struct LookAtInfo {
  glm::vec3 front;
  glm::vec3 right;
  glm::vec3 up;
  glm::mat4 transform;
};

class Camera {
 public:
  Camera(float pitch, float yaw);
  Camera(float pitch, float yaw, glm::vec3 position);
  // Defaults to looking (0, 1, 0)
  Camera(glm::vec3 position);

  LookAtInfo GetLookAt();
  void Update(int xrel, int yrel, float radians_per_dot);

 private:
  glm::vec3 _position;
  float _pitch;	
  float _yaw;

};
	
}  // namespace aim