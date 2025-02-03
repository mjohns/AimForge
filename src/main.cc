#include "aim/core/application.h"
#include "aim/ui/home_screen.h"

int main(int, char**) {
  using namespace aim;
  auto app = Application::Create();

  HomeScreen home_screen;
  home_screen.Run(app.get());

  return 0;
}
