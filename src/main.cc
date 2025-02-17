#include "aim/core/application.h"
#include "aim/ui/ui_screen.h"

int main(int, char**) {
  using namespace aim;
  auto app = Application::Create();
  CreateHomeScreen(app.get())->Run();
  return 0;
}
