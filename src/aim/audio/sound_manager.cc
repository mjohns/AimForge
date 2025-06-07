#include "sound_manager.h"

#include "glm/common.hpp"

namespace aim {
namespace {

constexpr int kHitChannel = 1;
constexpr int kShootChannel = 2;
constexpr int kMetronomeChannel = 3;

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
    : sound_dirs_(sound_dirs) {}

void SoundManager::LoadSounds(const Settings& settings) {
  const SoundSettings& s = settings.sound();
  if (s.has_master_volume_level()) {
    float level = glm::clamp<float>(s.master_volume_level(), 0, 1);
    Mix_MasterVolume(level * MIX_MAX_VOLUME);
  }
  std::vector<std::string> sounds{
      s.hit(),
      s.kill(),
      s.metronome(),
      s.shoot(),
  };
  for (const std::string& name : sounds) {
    if (name.size() == 0) {
      continue;
    }
    auto it = sound_cache_.find(name);
    if (it == sound_cache_.end()) {
      std::unique_ptr<Sound> sound = LoadSound(sound_dirs_, name);
      sound_cache_[name] = std::move(sound);
    }
  }
}

SoundManager& SoundManager::PlayKillSound(const std::string& name) {
  PlaySound(name, -1);
  return *this;
}

void SoundManager::PlaySound(const std::string& name, int channel) {
  auto it = sound_cache_.find(name);
  if (it != sound_cache_.end()) {
    Sound* sound = it->second.get();
    if (sound != nullptr) {
      sound->Play(channel);
    }
  }
}

SoundManager& SoundManager::PlayHitSound(const std::string& name) {
  PlaySound(name, kHitChannel);
  return *this;
}

SoundManager& SoundManager::PlayShootSound(const std::string& name) {
  PlaySound(name, kShootChannel);
  return *this;
}

SoundManager& SoundManager::PlayMetronomeSound(const std::string& name) {
  PlaySound(name, kMetronomeChannel);
  return *this;
}

}  // namespace aim
