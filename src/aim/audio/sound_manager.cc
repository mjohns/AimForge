#include "sound_manager.h"

namespace aim {
namespace {

constexpr int kKillChannel = 1;
constexpr int kShootChannel = 2;
constexpr int kMetronomeChannel = 3;

}  // namespace

SoundManager::SoundManager() {
  std::string kill_sound_name = "kill_confirmed.ogg";
  std::string shoot_sound_name = "shoot.ogg";
  std::string metronome_sound_name = "short_bass.wav";

  auto kill_sound = Sound::Load(kill_sound_name);
  auto shoot_sound = Sound::Load(shoot_sound_name);
  auto metronome_sound = Sound::Load(metronome_sound_name);

  kill_sound_ = kill_sound.get();
  shoot_sound_ = shoot_sound.get();
  metronome_sound_ = metronome_sound.get();

  sound_cache_[kill_sound_name] = std::move(kill_sound);
  sound_cache_[shoot_sound_name] = std::move(shoot_sound);
  sound_cache_[metronome_sound_name] = std::move(metronome_sound);
}

SoundManager& SoundManager::PlayKillSound() {
  if (kill_sound_ != nullptr) {
    kill_sound_->Play(kKillChannel);
  }
  return *this;
}

SoundManager& SoundManager::PlayShootSound() {
  if (shoot_sound_ != nullptr) {
    shoot_sound_->Play(kShootChannel);
  }
  return *this;
}

SoundManager& SoundManager::PlayMetronomeSound() {
  if (metronome_sound_ != nullptr) {
    metronome_sound_->Play(kMetronomeChannel);
  }
  return *this;
}

}  // namespace aim
