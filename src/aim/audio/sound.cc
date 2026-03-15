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
  MIX_Track* track = MIX_CreateTrack(mixer);
  if (track == nullptr) {
    return {};
  }
  MIX_SetTrackAudio(track, audio);
  return std::unique_ptr<Sound>(new Sound(mixer, audio, track));
}

Sound::Sound(MIX_Mixer* mixer, MIX_Audio* audio, MIX_Track* track)
    : mixer_(mixer), audio_(audio), track_(track) {}

Sound::~Sound() {}

void Sound::Play(float gain) {
  // MIX_PlayAudio(mixer_, audio_);
  auto options = SDL_CreateProperties();
  MIX_SetTrackGain(track_, gain);
  MIX_PlayTrack(track_, options);
}

}  // namespace aim
