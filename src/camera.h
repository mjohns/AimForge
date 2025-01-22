#pragma once

#include "glm/vec3.hpp"
#include "glm/mat4x4.hpp"

namespace aim {


// Convert cm/360 to radians per pixel
float CmPer360ToRadiansPerPixel(float cm_per_360, float dpi);

class Camera {
 public:
  Camera(float pitch, float yaw);

  glm::mat4 GetLookAt();
  glm::vec3 GetRight();
  void Update(int xrel, int yrel, float radians_per_pixel);

 private:
  float _pitch;	
  float _yaw;

};
	
}  // namespace aim