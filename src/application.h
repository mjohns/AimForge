#pragma once

#include <SDL.h>
#include <SDL_vulkan.h>

#include <memory>

#include "backends/imgui_impl_vulkan.h"
#include "util.h"
#include "model.h"

namespace aim {

class Application {
 public:
  ~Application();

  static std::unique_ptr<Application> Create();

  void MaybeRebuildSwapChain();
  void FrameRender(ImVec4 clear_color, ImDrawData* draw_data);

  SDL_Window* GetSdlWindow() {
    return _sdl_window;
  }

  ScreenInfo GetScreenInfo() {
    return ScreenInfo(_window_width, _window_height);
  }

  Application(const Application&) = delete;
  Application(Application&&) = default;
  Application& operator=(Application other) = delete;
  Application& operator=(Application&& other) = delete;

 private:
  Application();

  int Initialize();
  void FramePresent();
  void SetupVulkan(ImVector<const char*> instance_extensions);
  void SetupVulkanWindow(VkSurfaceKHR surface);

  // Vulkan
  VkAllocationCallbacks* _allocator = nullptr;
  VkInstance _instance = VK_NULL_HANDLE;
  VkPhysicalDevice _physical_device = VK_NULL_HANDLE;
  VkDevice _device = VK_NULL_HANDLE;
  uint32_t _queue_family = (uint32_t)-1;
  VkQueue _queue = VK_NULL_HANDLE;
  VkPipelineCache _pipeline_cache = VK_NULL_HANDLE;
  VkDescriptorPool _descriptor_pool = VK_NULL_HANDLE;
  ImGui_ImplVulkanH_Window _main_window_data;
  bool _swap_chain_rebuild = false;

  SDL_Window* _sdl_window = nullptr;

  int _window_width = -1;
  int _window_height = -1;
};

}  // namespace aim