#include "sound.h"

#include <string>

namespace aim {

std::unique_ptr<Sound> Sound::Load(const std::filesystem::path& sound_path) {
  if (!std::filesystem::exists(sound_path)) {
    return {};
  }
  Mix_Chunk* chunk = Mix_LoadWAV(sound_path.string().c_str());
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