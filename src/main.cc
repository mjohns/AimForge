#include "aim/core/application.h"
#include "aim/ui/home_screen.h"

int main(int, char**) {
  using namespace aim;
  while (true) {
    try {
      auto app = Application::Create();
      app->PushScreenInternal(CreateHomeScreen(app.get()));
      app->RunMainLoop();
      return 0;
    } catch (ApplicationExitException e) {
      return 0;
    } catch (ApplicationRestartException e) {
      // Continue to next loop iteration.
    }
  }
  return 0;
}
