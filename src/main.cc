#include "aim/core/application.h"
#include "aim/core/process_lock.h"
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

  // Ensure only 1 instance of the program is running.
  auto process_lock = CreateProcessLock();

  while (true) {
    auto app = Application::Create();
    if (!app) {
      break;
    }

    app->PushScreenInternal(CreateHomeScreen(app.get()));
    bool should_exit = app->RunMainLoop();
    if (should_exit) {
      break;
    }
    // Continue to next loop iteration.
  }

#ifdef _WIN32
  CoUninitialize();
#endif

  return 0;
}
