#include "shader.h"

#include <iostream>

#include <glad/glad.h>

namespace aim {
namespace {
void checkCompileErrors(GLuint shader, std::string type) {
  GLint success;
  GLchar infoLog[1024];
  if (type != "PROGRAM") {
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
      glGetShaderInfoLog(shader, 1024, NULL, infoLog);
      std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n"
                << infoLog << "\n -- --------------------------------------------------- -- "
                << std::endl;
    }
  } else {
    glGetProgramiv(shader, GL_LINK_STATUS, &success);
    if (!success) {
      glGetProgramInfoLog(shader, 1024, NULL, infoLog);
      std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n"
                << infoLog << "\n -- --------------------------------------------------- -- "
                << std::endl;
    }
  }
}
}  // namespace

Shader::Shader(std::string vertex_shader, std::string fragment_shader) {
  const char *vShaderCode = vertex_shader.c_str();
  const char *fShaderCode = fragment_shader.c_str();

  // 2. compile shaders
  unsigned int vertex, fragment;
  // vertex shader
  vertex = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex, 1, &vShaderCode, NULL);
  glCompileShader(vertex);
  checkCompileErrors(vertex, "VERTEX");
  // fragment Shader
  fragment = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment, 1, &fShaderCode, NULL);
  glCompileShader(fragment);
  checkCompileErrors(fragment, "FRAGMENT");

  // shader Program
  id_ = glCreateProgram();
  glAttachShader(id_, vertex);
  glAttachShader(id_, fragment);
  glLinkProgram(id_);
  checkCompileErrors(id_, "PROGRAM");
  // delete the shaders as they're linked into our program now and no longer
  // necessery
  glDeleteShader(vertex);
  glDeleteShader(fragment);
}

void Shader::Use() {
  glUseProgram(id_);
}

void Shader::SetBool(const std::string &name, bool value) const {
  glUniform1i(glGetUniformLocation(id_, name.c_str()), (int)value);
}

void Shader::SetInt(const std::string &name, int value) const {
  glUniform1i(glGetUniformLocation(id_, name.c_str()), value);
}

void Shader::SetFloat(const std::string &name, float value) const {
  glUniform1f(glGetUniformLocation(id_, name.c_str()), value);
}

void Shader::SetVec2(const std::string &name, const glm::vec2 &value) const {
  glUniform2fv(glGetUniformLocation(id_, name.c_str()), 1, &value[0]);
}

void Shader::SetVec2(const std::string &name, float x, float y) const {
  glUniform2f(glGetUniformLocation(id_, name.c_str()), x, y);
}

void Shader::SetVec3(const std::string &name, const glm::vec3 &value) const {
  glUniform3fv(glGetUniformLocation(id_, name.c_str()), 1, &value[0]);
}

void Shader::SetVec3(const std::string &name, float x, float y, float z) const {
  glUniform3f(glGetUniformLocation(id_, name.c_str()), x, y, z);
}

void Shader::SetVec4(const std::string &name, const glm::vec4 &value) const {
  glUniform4fv(glGetUniformLocation(id_, name.c_str()), 1, &value[0]);
}

void Shader::SetVec4(const std::string &name, float x, float y, float z, float w) const {
  glUniform4f(glGetUniformLocation(id_, name.c_str()), x, y, z, w);
}

void Shader::SetMat2(const std::string &name, const glm::mat2 &mat) const {
  glUniformMatrix2fv(glGetUniformLocation(id_, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}

void Shader::SetMat3(const std::string &name, const glm::mat3 &mat) const {
  glUniformMatrix3fv(glGetUniformLocation(id_, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}

void Shader::SetMat4(const std::string &name, const glm::mat4 &mat) const {
  glUniformMatrix4fv(glGetUniformLocation(id_, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}

}  // namespace aim