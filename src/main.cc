#include "aim/core/application.h"
#include "aim/scenario/static_scenario.h"

int main(int, char**) {
  using namespace aim;
  auto app = Application::Create();

  StaticScenarioParams params;
  params.num_targets = 3;
  params.room_height = 150;
  params.room_width = 170;
  params.target_radius = 1.5;

  aim::StaticScenario s(params);
  s.Run(app.get());

  return 0;
}
