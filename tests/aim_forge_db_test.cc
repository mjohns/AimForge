#include "aim/database/aim_forge_db.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <string>

#include "aim/common/log.h"
#include "gmock/gmock.h"
#include "google/protobuf/message.h"
#include "gtest/gtest.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"

using namespace aim;
using google::protobuf::Message;
using ::protobuf_matchers::EqualsProto;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Gt;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::ResultOf;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

namespace {}  // namespace

class AimForgeDbTest : public ::testing::Test {
 protected:
  std::filesystem::path temp_db_path_;
  std::filesystem::path db_path_;
  std::unique_ptr<AimForgeDb> db_;

  void SetUp() override {
    std::filesystem::path base_temp_path = std::filesystem::temp_directory_path();

    auto now = std::chrono::high_resolution_clock::now();
    auto timestamp =
        std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(1000, 9999);
    int random_suffix = distrib(gen);

    std::string db_file_name = "gtest_aim_forge_db_test_db_" + std::to_string(timestamp) + "_" +
                               std::to_string(random_suffix);
    db_path_ = base_temp_path / db_file_name;
    db_ = CreateAimForgeDb(db_path_);
  }

  void TearDown() override {
    if (db_) {
      db_ = {};
    }
    if (std::filesystem::exists(db_path_)) {
      std::filesystem::remove(db_path_);
    }
    Logger::getInstance().logger()->flush();
  }
};

TEST_F(AimForgeDbTest, GetScenarioNameMap_NoEntries) {
  EXPECT_THAT(db_->GetScenarioIdMap(), IsEmpty());
}

TEST_F(AimForgeDbTest, GetScenarioNameMap) {
  i64 id1 = db_->CreateScenarioEntry("Scenario1");
  ASSERT_THAT(id1, Gt(0));
  i64 id2 = db_->CreateScenarioEntry("Scenario2");
  ASSERT_THAT(id2, Gt(0));
  i64 id3 = db_->CreateScenarioEntry("Scenario3");
  ASSERT_THAT(id3, Gt(0));
  i64 id4 = db_->CreateScenarioEntry("Scenario4");
  ASSERT_THAT(id4, Gt(0));

  EXPECT_THAT(db_->GetScenarioIdMap(),
              UnorderedElementsAre(Pair("Scenario1", id1),
                                   Pair("Scenario2", id2),
                                   Pair("Scenario3", id3),
                                   Pair("Scenario4", id4)));
}
