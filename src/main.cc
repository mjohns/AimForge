#include "application.h"
#include "model.h"

int main(int, char**) {
  auto app = aim::Application::Create();

  aim::Scenario s;
  s.Run(app.get());

  return 0;
}
