#include "sound.h"

#include <string>

#include "SDL3_mixer/SDL_mixer.h"

namespace aim {

std::unique_ptr<Sound> Sound::Load(MIX_Mixer* mixer, const std::filesystem::path& sound_path) {
  if (!std::filesystem::exists(sound_path)) {
    return {};
  }
  MIX_Audio* audio = MIX_LoadAudio(mixer, sound_path.string().c_str(), true);
  if (audio == nullptr) {
    return {};
  }
  return std::unique_ptr<Sound>(new Sound(mixer, audio));
}

Sound::Sound(MIX_Mixer* mixer, MIX_Audio* audio) : mixer_(mixer), audio_(audio) {}

Sound::~Sound() {}

void Sound::Play() {
  MIX_PlayAudio(mixer_, audio_);
}

}  // namespace aim
