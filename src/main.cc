#include "aim/core/application.h"
#include "aim/ui/home_screen.h"

int main(int, char**) {
  using namespace aim;
  try {
    auto app = Application::Create();
    app->PushScreen(CreateHomeScreen(app.get()));
    app->RunMainLoop();
  } catch (ApplicationExitException e) {
    return 0;
  }
  return 0;
}
