#pragma once

#include <absl/status/status.h>

#include <filesystem>

#include "aim/proto/settings.pb.h"

namespace aim {

class SettingsManager {
 public:
  explicit SettingsManager(const std::filesystem::path& settings_path);
  ~SettingsManager();

  absl::Status Initialize();

  FullSettings GetFullSettings();
  FullSettings* GetMutableFullSettings();
  float GetDpi();
  Settings GetCurrentSettings();
  Settings* GetMutableCurrentSettings();

  void MarkDirty();
  void FlushToDisk();
  // Only flush to disk if marked dirty.
  void MaybeFlushToDisk();

  SettingsManager(const SettingsManager&) = delete;
  SettingsManager(SettingsManager&&) = default;
  SettingsManager& operator=(SettingsManager other) = delete;
  SettingsManager& operator=(SettingsManager&& other) = delete;

 private:
  std::filesystem::path settings_path_;
  FullSettings full_settings_;
  bool needs_save_ = false;
};

}  // namespace aim
