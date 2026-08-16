#include "aim/core/scenario_manager.h"

#include <optional>

#include "aim/core/scenario_manager.h"
#include "aim/proto/scenario.pb.h"
#include "gmock/gmock.h"
#include "google/protobuf/message.h"
#include "gtest/gtest.h"
#include "protobuf-matchers/protocol-buffer-matchers.h"

using namespace aim;

using ::google::protobuf::Message;
using ::protobuf_matchers::EqualsProto;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Property;
using ::testing::StrEq;

TEST(ScenarioManagerTest, GetEvaluatedScenarioDef_SelfReference) {
  auto mgr = CreateScenarioManager();

  std::string name = "Scenario 1";

  ScenarioDef def;
  def.mutable_reference_def()->set_scenario_name(name);

  mgr->UpdateScenario(name, def);

  auto maybe_def = mgr->GetEvaluatedScenarioDef(name);
  ASSERT_THAT(maybe_def, Eq(std::nullopt));

  maybe_def = mgr->GetEvaluatedScenarioDef("Scenario 1 L2");
  ASSERT_THAT(maybe_def, Eq(std::nullopt));
}

TEST(ScenarioManagerTest, GetEvaluatedScenarioDef_SelfReferenceWithDynamicSuffix) {
  auto mgr = CreateScenarioManager();

  std::string name = "Scenario 1";

  ScenarioDef def;
  def.mutable_reference_def()->set_scenario_name("Scenario 1 40s");

  mgr->UpdateScenario(name, def);

  auto maybe_def = mgr->GetEvaluatedScenarioDef(name);
  ASSERT_THAT(maybe_def, Eq(std::nullopt));
}
