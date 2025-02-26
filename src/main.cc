#include "aim/core/application.h"
#include "aim/ui/app_ui.h"

int main(int, char**) {
  using namespace aim;
  auto app = Application::Create();

  try {
    CreateAppUi(app.get())->Run();
  } catch (ApplicationExitException e) {
    return 0;
  }

  return 0;
}
