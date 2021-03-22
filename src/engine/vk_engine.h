//
// Created by Martin Danhier on 15/03/2021.
//

#pragma once

#include "vk_types.h"
#include <vector>

class VulkanEngine {
private:
  // Attributes

  // == GENERAL ==

  /** Is the engine initialized properly ? */
  bool _isInitialized{false};
  /** Index of the current frame */
  int _frameNumber{1};

  // == WINDOWING ==

  /** Size of the main window */
  vk::Extent2D _windowExtent{950, 700};
  /** The main window */
  struct SDL_Window *_window{nullptr};
  /** Vulkan window surface */
  vk::SurfaceKHR _surface;

  // == VULKAN CORE RENDERING ==

  /** Vulkan library handle */
  vk::Instance _instance;
  /** Vulkan debug output handle */
  vk::DebugUtilsMessengerEXT _debugMessenger;
  /** GPU chosen as the default device */
  vk::PhysicalDevice _chosenGPU;
  /** Vulkan device for commands */
  vk::Device _device;
  /** Swapchain to render to the surface */
  vk::SwapchainKHR _swapchain;
  /** Image format expected by the windowing system */
  vk::Format _swapchainImageFormat;
  /** Array of images from the swapchain */
  std::vector<vk::Image> _swapchainImages;
  /** Array of image views from the swapchain */
  std::vector<vk::ImageView> _swapchainImageViews;
  /** Queue used for rendering */
  vk::Queue _graphicsQueue;
  /** Family of the graphics queue */
  uint32_t _graphicsQueueFamily;
  /** Command pool used to record commands */
  vk::CommandPool _commandPool;
  /** Command buffer used to record commands into */
  vk::CommandBuffer _mainCommandBuffer;
  /** Render pass */
  vk::RenderPass _renderPass;
  /** Framebuffers */
  std::vector<vk::Framebuffer> _framebuffers;

  // == SYNCHRONIZATION ==
  vk::Semaphore _presentSemaphore;
  vk::Semaphore _renderSemaphore;
  vk::Fence _renderFence;

  // Methods
  void InitVulkan();
  void InitSwapchain();
  void InitCommands();
  void InitDefaultRenderPass();
  void InitFramebuffers();
  void InitSyncStructures();


public:
  /**
   * Initializes everything in the engine
   */
  void Init();

  /**
   * Shuts down the engine
   */
  void Cleanup();

  /**
   * Draw loop
   */
  void Draw();

  /**
   * Run main loop
   */
  void Run();
};
