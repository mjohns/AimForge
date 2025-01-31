#include "aim/application.h"
#include "aim/model.h"

int main(int, char**) {
  using namespace aim;
  auto app = Application::Create();

  StaticWallParams params;
  params.num_targets = 3;
  params.height = 150;
  params.width = 170;
  params.target_radius = 1.5;

  std::unique_ptr<ScenarioDef> def = std::make_unique<StaticWallScenarioDef>(params);
  aim::Scenario s(def.get());
  s.Run(app.get());

  return 0;
}
