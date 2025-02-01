#include "sound.h"

#include <string>

namespace aim {
namespace {
const std::string kSoundBasePath = "C:/Users/micha/src/aim-trainer/sounds/";
}  // namespace

std::unique_ptr<Sound> Sound::Load(std::string sound_name) {
  std::string path = kSoundBasePath + sound_name;
  Mix_Music* music = Mix_LoadMUS(path.c_str());
  if (music == nullptr) {
    return {};
  }
  return std::unique_ptr<Sound>(new Sound(music));
}

Sound::Sound(Mix_Music* music) : _music(music) {}

Sound::~Sound() {
  if (_music != nullptr) {
    Mix_FreeMusic(_music);
  }
}

void Sound::Play() {
  Mix_PlayMusic(_music, 1);
}

Sounds GetDefaultSounds() {
  Sounds s;
  s.shoot = Sound::Load("shoot.ogg");
  s.kill = Sound::Load("kill_confirmed.ogg");
  return s;
}

}  // namespace aim