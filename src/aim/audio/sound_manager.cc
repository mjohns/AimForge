#include "sound_manager.h"

#include "SDL3_mixer/SDL_mixer.h"
#include "absl/algorithm/container.h"
#include "glm/common.hpp"

namespace aim {
namespace {

constexpr int kHitChannel = 1;
constexpr int kShootChannel = 2;
constexpr int kMetronomeChannel = 3;
constexpr int kKillChannel = 4;

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

std::vector<std::string> SoundManager::ListSounds() {
  std::unordered_set<std::string> sound_names;
  for (const std::filesystem::path& dir : sound_dirs_) {
    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
      continue;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
      std::string filename = entry.path().filename().string();
      if (filename.ends_with(".ogg")) {
        sound_names.insert(filename);
      }
    }
  }

  std::vector<std::string> sorted_sound_names(sound_names.begin(), sound_names.end());
  absl::c_sort(sorted_sound_names);
  return sorted_sound_names;
}

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
    MaybeLoadSound(name);
  }
}

SoundManager& SoundManager::PlayKillSound(const std::string& name) {
  PlaySound(name, kKillChannel);
  return *this;
}

bool SoundManager::LoadAndPlaySound(const std::string& name, int channel) {
  MaybeLoadSound(name);
  return PlaySound(name, channel);
}

void SoundManager::MaybeLoadSound(const std::string& sound_name) {
  if (sound_name.empty()) {
    return;
  }
  auto it = sound_cache_.find(sound_name);
  if (it == sound_cache_.end()) {
    std::unique_ptr<Sound> sound = LoadSound(sound_dirs_, sound_name);
    sound_cache_[sound_name] = std::move(sound);
  }
}

bool SoundManager::PlaySound(const std::string& name, int channel) {
  if (name.empty()) {
    return false;
  }
  auto it = sound_cache_.find(name);
  if (it != sound_cache_.end()) {
    Sound* sound = it->second.get();
    if (sound != nullptr) {
      sound->Play(channel);
      return true;
    }
  }

  return false;
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
