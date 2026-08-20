#include "SDL3/SDL.h"  // IWYU pragma: keep
#include "absl/cleanup/cleanup.h"
#include "absl/flags/parse.h"
#include "aim/core/application.h"
#include "aim/core/process_lock.h"
#include "aim/ui/home_screen.h"
#include "aim/ui/ui_app.h"

#ifdef _WIN32
#include <Windows.h>
#include <objbase.h>
#endif

using namespace aim;

int main(int argc, char* argv[]) {
  absl::ParseCommandLine(argc, argv);
  std::locale::global(std::locale("en_US.UTF-8"));

#ifdef _WIN32
  HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  if (!SUCCEEDED(hr)) {
    return -1;
  }
  auto co_init_cleanup = absl::MakeCleanup([]() { CoUninitialize(); });
#endif

  // Ensure only 1 instance of the program is running.
  auto process_lock = CreateProcessLock();
  auto existing_pid_with_lock = process_lock->CreateLockFile();
  if (existing_pid_with_lock) {
    std::string error_msg =
        std::format("An existing instance of FpsAimForge is already running with process id: {}",
                    *existing_pid_with_lock);
    SDL_ShowSimpleMessageBox(
        SDL_MESSAGEBOX_ERROR, "FpsAimForge already open", error_msg.c_str(), nullptr);
    return 0;
  }
  auto pid_cleanup = absl::MakeCleanup([&]() { process_lock->ReleaseLockFile(); });

  while (true) {
    auto app = Application::Create();
    if (!app) {
      break;
    }
    UiAppHolder::getInstance().set(app.get());

    app->PushScreenInternal(CreateHomeScreen());
    bool should_exit = app->RunMainLoop();
    if (should_exit) {
      break;
    }

    // Continue to next loop iteration.
    app = {};
    UiAppHolder::getInstance().set(nullptr);
  }

  return 0;
}
