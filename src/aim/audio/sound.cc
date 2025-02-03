#include "sound.h"

#include <string>

namespace aim {
namespace {
const std::string kSoundBasePath = "C:/Users/micha/src/aim-trainer/sounds/";
}  // namespace

std::unique_ptr<Sound> Sound::Load(std::string sound_name) {
  std::string path = kSoundBasePath + sound_name;
  Mix_Chunk* chunk = Mix_LoadWAV(path.c_str());
  if (chunk == nullptr) {
    return {};
  }
  return std::unique_ptr<Sound>(new Sound(chunk));
}

Sound::Sound(Mix_Chunk* chunk) : chunk_(chunk) {}

Sound::~Sound() {
  if (chunk_ != nullptr) {
    Mix_FreeChunk(chunk_);
  }
}

void Sound::Play(int channel) {
  Mix_PlayChannel(channel, chunk_, 0);
}

}  // namespace aim