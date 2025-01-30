#include "glad_loader.h"

#include <SDL3/SDL_video.h>
#include <glad/glad.h>

namespace aim {

int LoadGlad() {
  return gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress);
}

}  // namespace aim
