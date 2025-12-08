#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

#include "aim/common/simple_types.h"

namespace aim {

class AimForgeDb {
 public:
  virtual ~AimForgeDb() {}

  virtual i64 CreateScenarioEntry(const std::string& name) = 0;

  virtual std::unordered_map<std::string, i64> GetScenarioIdMap() = 0;
};

std::unique_ptr<AimForgeDb> CreateAimForgeDb(const std::filesystem::path& db_path);

}  // namespace aim
