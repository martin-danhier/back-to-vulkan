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

  // == test ==
  vk::PipelineLayout _trianglePipelineLayout;
  vk::Pipeline _trianglePipeline;

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
  void InitPipelines();
  vk::ShaderModule LoadShaderModule(const char* filePath);


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

class PipelineBuilder {
private:
  std::vector<vk::PipelineShaderStageCreateInfo> _shaderStages;
  vk::PipelineVertexInputStateCreateInfo _vertexInputInfo;
  vk::PipelineInputAssemblyStateCreateInfo _inputAssembly;
  vk::Viewport _viewport;
  vk::Rect2D _scissor;
  vk::PipelineRasterizationStateCreateInfo _rasterizer;
  vk::PipelineColorBlendAttachmentState _colorBlendAttachment;
  vk::PipelineMultisampleStateCreateInfo _multisampling;
  vk::PipelineLayout _pipelineLayout;
  // Booleans to store whether default should be applied or not
  bool _rasterizerInited = false;
  bool _inputAssemblyInited = false;
  bool _vertexInputInited = false;
#ifndef NDEBUG
  bool _pipelineLayoutInited = false;
  bool _scissorsInited = false;
  bool _viewportInited = false;
#endif

public:
  PipelineBuilder AddShaderStage(vk::ShaderStageFlagBits stage, vk::ShaderModule shaderModule);
  PipelineBuilder WithVertexInput();
  PipelineBuilder WithAssemblyTopology(vk::PrimitiveTopology topology);
  PipelineBuilder WithPolygonMode(vk::PolygonMode polygonMode);
  PipelineBuilder WithPipelineLayout(vk::PipelineLayout pipelineLayout);
  PipelineBuilder WithScissors(int32_t xOffset, int32_t yOffset, vk::Extent2D extent);
  PipelineBuilder WithScissors(vk::Rect2D scissors);
  PipelineBuilder WithViewport(float x, float y, float width, float height, float minDepth, float maxDepth);
  PipelineBuilder WithViewport(vk::Viewport viewport);
  PipelineBuilder GetDefaultsForExtent(vk::Extent2D windowExtent);
  vk::Pipeline Build(vk::Device device, vk::RenderPass pass);
};