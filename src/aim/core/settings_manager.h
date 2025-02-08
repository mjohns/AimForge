#pragma once

#include <filesystem>

#include "aim/fbs/settings_generated.h"

namespace aim {

class SettingsManager {
 public:
  explicit SettingsManager(const std::filesystem::path& settings_path);
  ~SettingsManager();

  FullSettingsT GetFullSettings();
  FullSettingsT* GetMutableFullSettings();
  float GetDpi();
  SettingsT GetCurrentSettings();
  SettingsT* GetMutableCurrentSettings();

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
  FullSettingsT full_settings_;
  bool needs_save_ = false;
};

}  // namespace aim
