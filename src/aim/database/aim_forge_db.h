#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "aim/common/simple_types.h"
#include "aim/proto/settings.pb.h"

namespace aim {

class AimForgeDb {
 public:
  virtual ~AimForgeDb() {}

  virtual i64 CreateScenarioEntry(const std::string& name,
                                  std::optional<ScenarioSettings> settings = {}) = 0;
  virtual ScenarioSettings GetScenarioSettings(i64 scenario_id) = 0;
  virtual std::unordered_map<std::string, i64> GetScenarioIdMap() = 0;
};

std::unique_ptr<AimForgeDb> CreateAimForgeDb(const std::filesystem::path& db_path);

}  // namespace aim
