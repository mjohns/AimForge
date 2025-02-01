#pragma once

#include <SDL3_mixer/SDL_mixer.h>

#include <memory>
#include <string>

namespace aim {

class Sound {
 public:
  static std::unique_ptr<Sound> Load(std::string sound_name);
  ~Sound();

  void Play(int channel);

  Sound(const Sound&) = delete;
  Sound(Sound&&) = default;
  Sound& operator=(Sound other) = delete;
  Sound& operator=(Sound&& other) = delete;

 private:
  Sound(Mix_Chunk* chunk);
  Mix_Chunk* _chunk;
};

}  // namespace aim