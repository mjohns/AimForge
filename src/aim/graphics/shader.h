#pragma once

#include <glm/glm.hpp>
#include <string>

namespace aim {

class Shader {
 public:
  Shader(std::string vertex_shader, std::string fragment_shader);

  void Use();
  void SetBool(const std::string& name, bool value) const;
  void SetInt(const std::string& name, int value) const;
  void SetFloat(const std::string& name, float value) const;
  void SetVec2(const std::string& name, const glm::vec2 &value) const;
  void SetVec2(const std::string& name, float x, float y) const;
  void SetVec3(const std::string& name, const glm::vec3 &value) const;
  void SetVec3(const std::string& name, float x, float y, float z) const;
  void SetVec4(const std::string& name, const glm::vec4 &value) const;
  void SetVec4(const std::string& name, float x, float y, float z, float w) const;
  void SetMat2(const std::string& name, const glm::mat2 &mat) const;
  void SetMat3(const std::string& name, const glm::mat3 &mat) const;
  void SetMat4(const std::string& name, const glm::mat4 &mat) const;

 private:
  unsigned int id_;
};

}  // namespace aim