#include "aim/core/application.h"
#include "aim/ui/ui_screen.h"
#include "aim/common/geometry.h";

int main(int, char**) {
  using namespace aim;
  auto app = Application::Create();

  for (int i = 0; i < 100000; ++i) {
    float x_radius = 160;
    float y_radius = 210;
    auto p = GetRandomPositionInEllipse(x_radius, y_radius, app->random_generator());
    if (!IsPointInEllipse(p, x_radius * 1.1, y_radius)) {
      return -1;
    }
  }

  CreateHomeScreen(app.get())->Run();
  return 0;
}
