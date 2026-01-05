#include "aim/core/application.h"
#include "aim/ui/home_screen.h"

#ifdef _WIN32
#include <Windows.h>
#include <objbase.h>
#endif

int main(int, char**) {
  using namespace aim;
  std::locale::global(std::locale("en_US.UTF-8"));

#ifdef _WIN32
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (!SUCCEEDED(hr)) {
    return -1;
  }
#endif

  while (true) {
    try {
      auto app = Application::Create();
      app->PushScreenInternal(CreateHomeScreen(app.get()));
      app->RunMainLoop();
      break;
    } catch (ApplicationExitException e) {
      break;
    } catch (ApplicationRestartException e) {
      // Continue to next loop iteration.
    }
  }

#ifdef _WIN32
  CoUninitialize();
#endif

  return 0;
}
