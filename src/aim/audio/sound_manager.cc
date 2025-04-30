#include "sound_manager.h"

namespace aim {
namespace {

constexpr int kKillChannel = 1;
constexpr int kShootChannel = 2;
constexpr int kMetronomeChannel = 3;
constexpr int kHitChannel = 4;

std::unique_ptr<Sound> LoadSound(const std::vector<std::filesystem::path>& sound_dirs,
                                 const std::string& name) {
  for (const std::filesystem::path& dir : sound_dirs) {
    auto loaded_sound = Sound::Load(dir / name);
    if (loaded_sound) {
      return std::move(loaded_sound);
    }
  }

  return {};
}

}  // namespace

SoundManager::SoundManager(const std::vector<std::filesystem::path>& sound_dirs)
    : sound_dirs_(sound_dirs) {
  std::string kill_sound_name = "kill_confirmed.ogg";
  std::string notify_before_kill_sound_name = "short_bass.wav";
  std::string shoot_sound_name = "shoot.ogg";
  std::string metronome_sound_name = "metronome.ogg";
  std::string hit_sound_name = "body_shot.ogg";

  auto kill_sound = LoadSound(sound_dirs_, kill_sound_name);
  auto notify_before_kill_sound = LoadSound(sound_dirs_, notify_before_kill_sound_name);
  auto shoot_sound = LoadSound(sound_dirs_, shoot_sound_name);
  auto metronome_sound = LoadSound(sound_dirs, metronome_sound_name);
  auto hit_sound = LoadSound(sound_dirs, hit_sound_name);

  kill_sound_ = kill_sound.get();
  shoot_sound_ = shoot_sound.get();
  metronome_sound_ = metronome_sound.get();
  hit_sound_ = hit_sound.get();
  // TODO: Make it easier to use same file for multiple sounds.
  notify_before_kill_sound_ = kill_sound.get();

  sound_cache_[kill_sound_name] = std::move(kill_sound);
  sound_cache_[shoot_sound_name] = std::move(shoot_sound);
  sound_cache_[metronome_sound_name] = std::move(metronome_sound);
  sound_cache_[hit_sound_name] = std::move(hit_sound);
  sound_cache_[notify_before_kill_sound_name] = std::move(notify_before_kill_sound);
}

SoundManager& SoundManager::PlayKillSound() {
  if (kill_sound_ != nullptr) {
    kill_sound_->Play(kKillChannel);
  }
  return *this;
}

SoundManager& SoundManager::PlayHitSound() {
  if (hit_sound_ != nullptr) {
    hit_sound_->Play(kKillChannel);
  }
  return *this;
}

SoundManager& SoundManager::PlayNotifyBeforeKillSound() {
  if (notify_before_kill_sound_ != nullptr) {
    notify_before_kill_sound_->Play(kHitChannel);
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
