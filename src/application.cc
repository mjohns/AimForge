#include "application.h"

#include <fmt/core.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <stdio.h>
#include <stdlib.h>

#include <memory>

#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"
#include "util.h"

namespace aim {
namespace {
constexpr uint32_t kMinImageCount = 2;

static void check_vk_result(VkResult err) {
  if (err == VK_SUCCESS) return;
  fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
  if (err < 0) abort();
}

static bool IsExtensionAvailable(const ImVector<VkExtensionProperties>& properties,
                                 const char* extension) {
  for (const VkExtensionProperties& p : properties)
    if (strcmp(p.extensionName, extension) == 0) return true;
  return false;
}

}  // namespace

Application::Application() {
}

Application::~Application() {
  auto err = vkDeviceWaitIdle(_device);
  check_vk_result(err);
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  vkDestroyDescriptorPool(_device, _descriptor_pool, _allocator);
  vkDestroyDevice(_device, _allocator);
  vkDestroyInstance(_instance, _allocator);

  ImGui_ImplVulkanH_DestroyWindow(_instance, _device, &_main_window_data, _allocator);

  if (_sdl_window) {
    SDL_DestroyWindow(_sdl_window);
  }
  SDL_Quit();
}

int Application::Initialize() {
  // Setup SDL
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    printf("Error: SDL_Init(): %s\n", SDL_GetError());
    return -1;
  }

  // Create window with Vulkan graphics context
  SDL_WindowFlags window_flags =
      (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  _sdl_window = SDL_CreateWindow("AimTrainer", 0, 0, window_flags);
  if (_sdl_window == nullptr) {
    printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
    return -1;
  }

  SDL_GetWindowSize(_sdl_window, &_window_width, &_window_height);

  ImVector<const char*> extensions;
  {
    uint32_t sdl_extensions_count = 0;
    const char* const* sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&sdl_extensions_count);
    for (uint32_t n = 0; n < sdl_extensions_count; n++) extensions.push_back(sdl_extensions[n]);
  }
  SetupVulkan(extensions);

  // Create Window Surface
  VkSurfaceKHR surface;
  VkResult err;
  if (SDL_Vulkan_CreateSurface(_sdl_window, _instance, _allocator, &surface) == 0) {
    printf("Failed to create Vulkan surface.\n");
    return 1;
  }

  // Create Framebuffers
  ImGui_ImplVulkanH_Window* wd = &_main_window_data;
  SetupVulkanWindow(surface);

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();
  // ImGui::StyleColorsLight();

  // Setup Platform/Renderer backends
  ImGui_ImplSDL3_InitForVulkan(_sdl_window);
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = _instance;
  init_info.PhysicalDevice = _physical_device;
  init_info.Device = _device;
  init_info.QueueFamily = _queue_family;
  init_info.Queue = _queue;
  init_info.PipelineCache = _pipeline_cache;
  init_info.DescriptorPool = _descriptor_pool;
  init_info.RenderPass = wd->RenderPass;
  init_info.Subpass = 0;
  init_info.MinImageCount = kMinImageCount;
  init_info.ImageCount = wd->ImageCount;
  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  init_info.Allocator = _allocator;
  init_info.CheckVkResultFn = check_vk_result;
  ImGui_ImplVulkan_Init(&init_info);

  return 0;
}

void Application::SetupVulkan(ImVector<const char*> instance_extensions) {
  VkResult err;

  // Create Vulkan Instance
  {
    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    // Enumerate available extensions
    uint32_t properties_count;
    ImVector<VkExtensionProperties> properties;
    vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
    properties.resize(properties_count);
    err = vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, properties.Data);
    check_vk_result(err);

    // Enable required extensions
    if (IsExtensionAvailable(properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
      instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
    if (IsExtensionAvailable(properties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
      instance_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
      create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
#endif

    // Create Vulkan Instance
    create_info.enabledExtensionCount = (uint32_t)instance_extensions.Size;
    create_info.ppEnabledExtensionNames = instance_extensions.Data;
    err = vkCreateInstance(&create_info, _allocator, &_instance);
    check_vk_result(err);
  }

  // Select Physical Device (GPU)
  _physical_device = ImGui_ImplVulkanH_SelectPhysicalDevice(_instance);
  IM_ASSERT(_physical_device != VK_NULL_HANDLE);

  // Select graphics queue family
  _queue_family = ImGui_ImplVulkanH_SelectQueueFamilyIndex(_physical_device);
  IM_ASSERT(_queue_family != (uint32_t)-1);

  // Create Logical Device (with 1 queue)
  {
    ImVector<const char*> device_extensions;
    device_extensions.push_back("VK_KHR_swapchain");

    // Enumerate physical device extension
    uint32_t properties_count;
    ImVector<VkExtensionProperties> properties;
    vkEnumerateDeviceExtensionProperties(_physical_device, nullptr, &properties_count, nullptr);
    properties.resize(properties_count);
    vkEnumerateDeviceExtensionProperties(
        _physical_device, nullptr, &properties_count, properties.Data);
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
    if (IsExtensionAvailable(properties, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
      device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

    const float queue_priority[] = {1.0f};
    VkDeviceQueueCreateInfo queue_info[1] = {};
    queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info[0].queueFamilyIndex = _queue_family;
    queue_info[0].queueCount = 1;
    queue_info[0].pQueuePriorities = queue_priority;
    VkDeviceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
    create_info.pQueueCreateInfos = queue_info;
    create_info.enabledExtensionCount = (uint32_t)device_extensions.Size;
    create_info.ppEnabledExtensionNames = device_extensions.Data;
    err = vkCreateDevice(_physical_device, &create_info, _allocator, &_device);
    check_vk_result(err);
    vkGetDeviceQueue(_device, _queue_family, 0, &_queue);
  }

  // Create Descriptor Pool
  // If you wish to load e.g. additional textures you may need to alter pools sizes and maxSets.
  {
    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
         IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE},
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 0;
    for (VkDescriptorPoolSize& pool_size : pool_sizes)
      pool_info.maxSets += pool_size.descriptorCount;
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    err = vkCreateDescriptorPool(_device, &pool_info, _allocator, &_descriptor_pool);
    check_vk_result(err);
  }
}

// All the ImGui_ImplVulkanH_XXX structures/functions are optional helpers used by the demo.
// Your real engine/app may not use them.
void Application::SetupVulkanWindow(VkSurfaceKHR surface) {
  ImGui_ImplVulkanH_Window* wd = &_main_window_data;
  wd->Surface = surface;

  // Check for WSI support
  VkBool32 res;
  vkGetPhysicalDeviceSurfaceSupportKHR(_physical_device, _queue_family, wd->Surface, &res);
  if (res != VK_TRUE) {
    fprintf(stderr, "Error no WSI support on physical device 0\n");
    exit(-1);
  }

  // Select Surface Format
  const VkFormat requestSurfaceImageFormat[] = {VK_FORMAT_B8G8R8A8_UNORM,
                                                VK_FORMAT_R8G8B8A8_UNORM,
                                                VK_FORMAT_B8G8R8_UNORM,
                                                VK_FORMAT_R8G8B8_UNORM};
  const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
  wd->SurfaceFormat =
      ImGui_ImplVulkanH_SelectSurfaceFormat(_physical_device,
                                            wd->Surface,
                                            requestSurfaceImageFormat,
                                            (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat),
                                            requestSurfaceColorSpace);

  // Select Present Mode
  VkPresentModeKHR present_modes[] = {
      VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR};
  // FPS limited
  // VkPresentModeKHR present_modes[] = {VK_PRESENT_MODE_FIFO_KHR};

  wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(
      _physical_device, wd->Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));
  // printf("[vulkan] Selected PresentMode = %d\n", wd->PresentMode);

  // Create SwapChain, RenderPass, Framebuffer, etc.
  IM_ASSERT(kMinImageCount >= 2);
  ImGui_ImplVulkanH_CreateOrResizeWindow(_instance,
                                         _physical_device,
                                         _device,
                                         wd,
                                         _queue_family,
                                         _allocator,
                                         _window_width,
                                         _window_height,
                                         kMinImageCount);
}

void Application::FrameRender(ImVec4 clear_color, ImDrawData* draw_data) {
  ImGui_ImplVulkanH_Window* wd = &_main_window_data;

  wd->ClearValue.color.float32[0] = clear_color.x * clear_color.w;
  wd->ClearValue.color.float32[1] = clear_color.y * clear_color.w;
  wd->ClearValue.color.float32[2] = clear_color.z * clear_color.w;
  wd->ClearValue.color.float32[3] = clear_color.w;

  VkSemaphore image_acquired_semaphore =
      wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
  VkSemaphore render_complete_semaphore =
      wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
  VkResult err = vkAcquireNextImageKHR(_device,
                                       wd->Swapchain,
                                       UINT64_MAX,
                                       image_acquired_semaphore,
                                       VK_NULL_HANDLE,
                                       &wd->FrameIndex);
  if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) _swap_chain_rebuild = true;
  if (err == VK_ERROR_OUT_OF_DATE_KHR) return;
  if (err != VK_SUBOPTIMAL_KHR) check_vk_result(err);

  ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
  {
    err = vkWaitForFences(_device,
                          1,
                          &fd->Fence,
                          VK_TRUE,
                          UINT64_MAX);  // wait indefinitely instead of periodically checking
    check_vk_result(err);

    err = vkResetFences(_device, 1, &fd->Fence);
    check_vk_result(err);
  }
  {
    err = vkResetCommandPool(_device, fd->CommandPool, 0);
    check_vk_result(err);
    VkCommandBufferBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
    check_vk_result(err);
  }
  {
    VkRenderPassBeginInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = wd->RenderPass;
    info.framebuffer = fd->Framebuffer;
    info.renderArea.extent.width = wd->Width;
    info.renderArea.extent.height = wd->Height;
    info.clearValueCount = 1;
    info.pClearValues = &wd->ClearValue;
    vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
  }

  // Record dear imgui primitives into command buffer
  ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

  // Submit command buffer
  vkCmdEndRenderPass(fd->CommandBuffer);
  {
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &image_acquired_semaphore;
    info.pWaitDstStageMask = &wait_stage;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &fd->CommandBuffer;
    info.signalSemaphoreCount = 1;
    info.pSignalSemaphores = &render_complete_semaphore;

    err = vkEndCommandBuffer(fd->CommandBuffer);
    check_vk_result(err);
    err = vkQueueSubmit(_queue, 1, &info, fd->Fence);
    check_vk_result(err);
  }

  FramePresent();
}

void Application::FramePresent() {
  ImGui_ImplVulkanH_Window* wd = &_main_window_data;
  if (_swap_chain_rebuild) return;
  VkSemaphore render_complete_semaphore =
      wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
  VkPresentInfoKHR info = {};
  info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  info.waitSemaphoreCount = 1;
  info.pWaitSemaphores = &render_complete_semaphore;
  info.swapchainCount = 1;
  info.pSwapchains = &wd->Swapchain;
  info.pImageIndices = &wd->FrameIndex;
  VkResult err = vkQueuePresentKHR(_queue, &info);
  if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) _swap_chain_rebuild = true;
  if (err == VK_ERROR_OUT_OF_DATE_KHR) return;
  if (err != VK_SUBOPTIMAL_KHR) check_vk_result(err);
  wd->SemaphoreIndex =
      (wd->SemaphoreIndex + 1) % wd->SemaphoreCount;  // Now we can use the next set of semaphores
}

void Application::MaybeRebuildSwapChain() {
  if (_swap_chain_rebuild) {
    ImGui_ImplVulkan_SetMinImageCount(kMinImageCount);
    ImGui_ImplVulkanH_CreateOrResizeWindow(_instance,
                                           _physical_device,
                                           _device,
                                           &_main_window_data,
                                           _queue_family,
                                           _allocator,
                                           _window_width,
                                           _window_height,
                                           kMinImageCount);
    _main_window_data.FrameIndex = 0;
    _swap_chain_rebuild = false;
  }
}

std::unique_ptr<Application> Application::Create() {
  auto application = std::unique_ptr<Application>(new Application());
  int rc = application->Initialize();
  if (rc != 0) {
    exit(rc);
  }
  return application;
}

}  // namespace aim