//
// Created by Martin Danhier on 15/03/2021.
//

#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <VkBootstrap.h>
#include <iostream>
#include <string>

#include "vk_init.h"
#include "vk_types.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

void VulkanEngine::Init() {
  // Initialize SDL
  SDL_Init(SDL_INIT_VIDEO);

  // Create a window
  SDL_WindowFlags windowFlags = SDL_WINDOW_VULKAN;
  _window = SDL_CreateWindow("Back to Vulkan !", SDL_WINDOWPOS_UNDEFINED,
                             SDL_WINDOWPOS_UNDEFINED, _windowExtent.width,
                             _windowExtent.height, windowFlags);

  // Load the core Vulkan structures
  InitVulkan();

  // Initialize swapchain
  InitSwapchain();

  // Initialize commands
  InitCommands();

  // Initialize render pass
  InitDefaultRenderPass();

  // Initialize framebuffers
  InitFramebuffers();

  // Initialize synchronisation structures
  InitSyncStructures();

  // Set the _isInitialized property to true if everything went fine
  _isInitialized = true;
}

void VulkanEngine::InitVulkan() {

  // === Instance creation ===

  // Init dynamic loader
  vk::DynamicLoader loader;
  auto vkGetInstanceProcAddr =
      loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
  VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

  // Create instance
  vkb::InstanceBuilder builder;
#ifndef NDEBUG
  // Enable validation layers and debug messenger in debug mode
  builder.request_validation_layers(true).use_default_debug_messenger();
#endif
  // Setup instance
  auto vkbInstance = builder.set_app_name("Back to Vulkan")
                         .set_app_version(0, 1, 0)
                         .set_engine_name("MyEngine")
                         .set_engine_version(0, 1, 0)
                         .require_api_version(1, 1, 0)
                         .build()
                         .value();

  // Wrap the instance and debug messenger in vk-hpp handle classes and store
  // them
  _instance = vk::Instance(vkbInstance.instance);
  _debugMessenger = vk::DebugUtilsMessengerEXT(vkbInstance.debug_messenger);

  // Initialize function pointers for instance
  VULKAN_HPP_DEFAULT_DISPATCHER.init(_instance);

  // Get the surface of the SDL window
  VkSurfaceKHR surface;
  SDL_Vulkan_CreateSurface(_window, _instance, &surface);
  // Save it in the vk-hpp handle class
  _surface = vk::SurfaceKHR(surface);

  // Select a GPU
  // We want a GPU that can write to the SDL surface and supports Vulkan 1.1
  vkb::PhysicalDeviceSelector gpuSelector{vkbInstance};
  auto vkbPhysicalDevice = gpuSelector.set_minimum_version(1, 1)
                               .set_surface(_surface)
                               .select()
                               .value();

  // Get logical device
  vkb::DeviceBuilder deviceBuilder{vkbPhysicalDevice};
  auto vkbDevice = deviceBuilder.build().value();

  // Save devices
  _device = vk::Device(vkbDevice.device);
  _chosenGPU = vk::PhysicalDevice(vkbPhysicalDevice.physical_device);

  // Initialize function pointers for device
  VULKAN_HPP_DEFAULT_DISPATCHER.init(_device);

  _graphicsQueue =
      vk::Queue(vkbDevice.get_queue(vkb::QueueType::graphics).value());
  _graphicsQueueFamily =
      vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
}

void VulkanEngine::InitSwapchain() {
  // Initialize swapchain with vkb
  vkb::SwapchainBuilder swapchainBuilder{_chosenGPU, _device, _surface};
  vk::SurfaceFormatKHR format{
      .format = vk::Format::eB8G8R8A8Unorm,
      .colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear,
  };
  auto vkbSwapchain =
      swapchainBuilder.set_desired_format(format)
          .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
          .set_desired_extent(_windowExtent.width, _windowExtent.height)
          .build()
          .value();

  //   Store swapchain and images
  _swapchain = vk::SwapchainKHR(vkbSwapchain.swapchain);
  _swapchainImages = _device.getSwapchainImagesKHR(_swapchain);
  _swapchainImageFormat = vk::Format(vkbSwapchain.image_format);

  //   Get image views
  _swapchainImageViews.resize(_swapchainImages.size());

  for (int i = 0; i < _swapchainImages.size(); i++) {
    vk::ImageViewCreateInfo createInfo{
        .image = _swapchainImages[i],
        .viewType = vk::ImageViewType::e2D,
        .format = _swapchainImageFormat,
        .components{
            .r = vk::ComponentSwizzle::eIdentity,
            .g = vk::ComponentSwizzle::eIdentity,
            .b = vk::ComponentSwizzle::eIdentity,
            .a = vk::ComponentSwizzle::eIdentity,
        },
        .subresourceRange{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    // Store it
    _swapchainImageViews[i] = _device.createImageView(createInfo);
  }
}

void VulkanEngine::InitCommands() {
  // Create a command pool
  vk::CommandPoolCreateInfo commandPoolCreateInfo{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = _graphicsQueueFamily,
  };
  _commandPool = _device.createCommandPool(commandPoolCreateInfo);

  // Create a primary command buffer
  vk::CommandBufferAllocateInfo commandBufferAllocateInfo{
      .commandPool = _commandPool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1,
  };
  _mainCommandBuffer =
      _device.allocateCommandBuffers(commandBufferAllocateInfo)[0];
}

void VulkanEngine::InitDefaultRenderPass() {
  // Define the target
  vk::AttachmentDescription colorAttachment{
      // The attachment must use the same format as the swapchain
      .format = _swapchainImageFormat,
      // No multisampling, so 1 sample
      .samples = vk::SampleCountFlagBits::e1,
      // clear on load
      .loadOp = vk::AttachmentLoadOp::eClear,
      // Keep the attachment stored when the renderpass ends
      .storeOp = vk::AttachmentStoreOp::eStore,
      // No stencil
      .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
      .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
      // Don't care about the starting layout of the attachment
      .initialLayout = vk::ImageLayout::eUndefined,
      // Once the renderpass ends, the image should be in a format ready for
      // presenting
      .finalLayout = vk::ImageLayout::ePresentSrcKHR,
  };

  // Reference to the attachment in the renderpass (sort of smart pointer)
  vk::AttachmentReference colorAttachmentRef{
      .attachment = 0, // Index of the colorAttachment in the renderpass
      .layout = vk::ImageLayout::eColorAttachmentOptimal,
  };

  // Create the subpass
  vk::SubpassDescription subpass{
      .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachmentRef,
  };

  // Create the render pass
  vk::RenderPassCreateInfo renderPassCreateInfo{
      .attachmentCount = 1,
      .pAttachments = &colorAttachment,
      .subpassCount = 1,
      .pSubpasses = &subpass,
  };
  _renderPass = _device.createRenderPass(renderPassCreateInfo);
}

void VulkanEngine::InitFramebuffers() {

  // Create the framebuffers
  vk::FramebufferCreateInfo framebufferCreateInfo{
      .renderPass = _renderPass,
      .attachmentCount = 1,
      .width = _windowExtent.width,
      .height = _windowExtent.height,
      .layers = 1,
  };

  const uint32_t swapchainImageCount = _swapchainImages.size();

  // Init the framebuffers array
  _framebuffers = std::vector<vk::Framebuffer>(swapchainImageCount);
  for (int i = 0; i < swapchainImageCount; ++i) {
    // Link the corresponding image
    framebufferCreateInfo.pAttachments = &_swapchainImageViews[i];
    // Create the framebuffer and store it in the array
    _framebuffers[i] = _device.createFramebuffer(framebufferCreateInfo);
  }
}

void VulkanEngine::InitSyncStructures() {
  // Create render fence
  vk::FenceCreateInfo fenceCreateInfo{
      .flags = vk::FenceCreateFlagBits::eSignaled,
  };
  _renderFence = _device.createFence(fenceCreateInfo);

  // Create semaphores
  _presentSemaphore = _device.createSemaphore({});
  _renderSemaphore = _device.createSemaphore({});
}

void VulkanEngine::Cleanup() {
  if (_isInitialized) {

    // Destroy everything in the opposite order as the creation order

    // Destroy sync structures
    _device.destroyFence(_renderFence);
    _device.destroySemaphore(_renderSemaphore);
    _device.destroySemaphore(_presentSemaphore);

    // Destroy command pool
    _device.destroyCommandPool(_commandPool);

    // Destroy swapchain
    _device.destroySwapchainKHR(_swapchain);

    // Destroy renderpass
    _device.destroyRenderPass(_renderPass);

    // Destroy swapchain resources
    for (int i = 0; i < _framebuffers.size(); i++) {
      _device.destroyFramebuffer(_framebuffers[i]);
      _device.destroyImageView(_swapchainImageViews[i]);
    }

    // Cleanup Vulkan
    _device.destroy();
    _instance.destroySurfaceKHR(_surface);
#ifndef NDEBUG
    _instance.destroyDebugUtilsMessengerEXT(_debugMessenger);
#endif
    _instance.destroy();

    // Destroy SDL window
    SDL_DestroyWindow(_window);
  }
}

void VulkanEngine::Draw() {
  // Wait until the GPU has finished rendering the last frame. Timeout of 1
  // second
  auto waitResult = _device.waitForFences(_renderFence, true, 1000000000);
  if (waitResult != vk::Result::eSuccess)
    throw std::runtime_error("Error while waiting for fences");
  _device.resetFences(_renderFence);

  // Request image index from swapchain
  uint32_t swapchainImageIndex;
  auto nextImageResult = _device.acquireNextImageKHR(_swapchain, 1000000000,_presentSemaphore);
  switch (nextImageResult.result) {
    // Success, keep it
  case vk::Result::eSuccess:
    swapchainImageIndex = nextImageResult.value;
    break;
    // Other "success" values, throw an error because I'm too lazy to handle
    // them now
  case vk::Result::eTimeout:
  case vk::Result::eNotReady:
  case vk::Result::eSuboptimalKHR:
    throw std::runtime_error("Error while getting next image");
    // Default: error. vk-hpp already throws an exception, so there is nothing
    // to do
  default:
    break;
  }

  // Reset the command buffer
  _mainCommandBuffer.reset();

  // Begin the recording of commands into the buffer
  vk::CommandBufferBeginInfo cmdBeginInfo{
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
  };
  _mainCommandBuffer.begin(cmdBeginInfo);

  // Define a clear color from frame number
  float flash = abs(sin(static_cast<float_t>(_frameNumber) / 120.f));
  float flash2 = abs(sin(static_cast<float_t>(_frameNumber) / 180.f));
  vk::ClearValue clearValue(vkinit::GetColor(1-flash, flash2, flash));

  // Start the main renderpass
  vk::RenderPassBeginInfo rpBeginInfo{
      .renderPass = _renderPass,
      .framebuffer = _framebuffers[swapchainImageIndex],
      .renderArea =
          {
              .offset = {.x = 0, .y = 0},
              .extent = _windowExtent,
          },
      .clearValueCount = 1,
      .pClearValues = &clearValue,
  };
  _mainCommandBuffer.beginRenderPass(rpBeginInfo, vk::SubpassContents::eInline);

  // End the renderpass to finish rendering commands
  _mainCommandBuffer.endRenderPass();
  // End the command buffer to finish it and prepare it to be submitted
  _mainCommandBuffer.end();

  vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

  // Submit the buffer to the queue
  vk::SubmitInfo submitInfo {
      // Wait until the image to render to is ready
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &_presentSemaphore,
      // Pipeline stage
      .pWaitDstStageMask = &waitStage,
      // Link the command buffer
      .commandBufferCount = 1,
      .pCommandBuffers = &_mainCommandBuffer,
      // Signal the render semaphore
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &_renderSemaphore,
  };
  _graphicsQueue.submit(submitInfo, _renderFence);

  // Present the image on the screen
  vk::PresentInfoKHR presentInfo {
      // Wait until the rendering is complete
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &_renderSemaphore,
      // Specify the swapchain to present
      .swapchainCount = 1,
      .pSwapchains = &_swapchain,
      // Specify the index of the image
      .pImageIndices = &swapchainImageIndex,
  };
  _graphicsQueue.presentKHR(presentInfo);

  // Increase the number of frames drawn
  _frameNumber++;

}

void VulkanEngine::Run() {
  // Init local variables
  SDL_Event event;
  bool shouldQuit = false;

  // Main loop
  while (!shouldQuit) {

    // Handle window event in a queue
    while (SDL_PollEvent(&event) != 0) {
      // Quit event: set shouldQuit to true to exit the main loop
      if (event.type == SDL_QUIT) {
        shouldQuit = true;
      }
      // Keypress event
      else if (event.type == SDL_KEYDOWN) {
        std::cout << event.key.keysym.sym << std::endl;
      }
    }

    // Run the rendering code
    Draw();
  }
}
