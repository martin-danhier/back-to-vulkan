//
// Created by Martin Danhier on 15/03/2021.
//

#pragma once

#include "Mesh.h"
#include "vk_types.h"
#include <deque>
#include <glm/glm.hpp>
#include <vector>

struct FrameData {
  vk::Semaphore presentSemaphore, renderSemaphore;
  vk::Fence renderFence;
  vk::CommandPool commandPool;
  vk::CommandBuffer mainCommandBuffer;
};

struct MeshPushConstants {
  glm::vec4 data;
  glm::mat4 render_matrix;
};

class DeletionQueue {
  std::deque<std::function<void()>> _deletors;

public:
  void PushFunction(std::function<void()> &&ppFunction);
  void Flush();
};

struct Material {
  vk::Pipeline pipeline;
  vk::PipelineLayout pipelineLayout;
};
struct RenderObject {
  Mesh* mesh;
  Material* material;
  glm::mat4 transformMatrix;
};

constexpr uint32_t FRAME_OVERLAP = 2;

class VulkanEngine {
private:
  // Attributes

  // == GENERAL ==

  /** Is the engine initialized properly ? */
  bool _isInitialized{false};
  /** Index of the current frame */
  int _frameNumber{1};
  double_t _deltaTime = 0;
  /** Deletion queue handling object deletion */
  DeletionQueue _mainDeletionQueue;
  /** Memory allocator */
  VmaAllocator _allocator;

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
  vk::Format _depthImageFormat;
  /** Array of images from the swapchain */
  std::vector<vk::Image> _swapchainImages;
  /** Array of image views from the swapchain */
  std::vector<vk::ImageView> _swapchainImageViews;
  AllocatedImage _depthImage;
  vk::ImageView _depthImageView;
  /** Queue used for rendering */
  vk::Queue _graphicsQueue;
  /** Family of the graphics queue */
  uint32_t _graphicsQueueFamily;
  /** Render pass */
  vk::RenderPass _renderPass;
  /** Framebuffers */
  std::vector<vk::Framebuffer> _framebuffers;
  FrameData _frames[FRAME_OVERLAP];

  // == Scene ==
  std::vector<RenderObject> _renderables;
  std::unordered_map<std::string, Material> _materials;
  std::unordered_map<std::string, Mesh> _meshes;

  // == Camera ==
  glm::vec3 _cameraMotion {0.0f};
  glm::vec3 _cameraPosition;

  // Shader switching
  int32_t _selectedShader = 0;

  // Methods
  void InitVulkan();
  void InitSwapchain();
  void InitCommands();
  void InitDefaultRenderPass();
  void InitFramebuffers();
  void InitSyncStructures();
  void InitPipelines();
  void LoadMeshes();
  void InitScene();
  void DrawObjects(vk::CommandBuffer cmd, RenderObject* first, int32_t count);
  vk::ShaderModule LoadShaderModule(const char *filePath);
  FrameData& GetCurrentFrame();

  Material* CreateMaterial(vk::Pipeline pipeline, vk::PipelineLayout layout, const std::string& name);
  Material* GetMaterial(const std::string& name);
  Mesh* GetMesh(const std::string& name);
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
  vk::PipelineDepthStencilStateCreateInfo _depthStencilCreateInfo;
  // Booleans to store whether default should be applied or not
  bool _rasterizerInited = false;
  bool _inputAssemblyInited = false;
  bool _vertexInputInited = false;
  bool _depthSettingsProvided = false;
#ifndef NDEBUG
  bool _pipelineLayoutInited = false;
  bool _scissorsInited = false;
  bool _viewportInited = false;
#endif

public:
  PipelineBuilder AddShaderStage(vk::ShaderStageFlagBits stage,
                                 vk::ShaderModule shaderModule);
  PipelineBuilder
  WithVertexInput(const VertexInputDescription &vertexInputDescription);
  PipelineBuilder WithAssemblyTopology(vk::PrimitiveTopology topology);
  PipelineBuilder WithPolygonMode(vk::PolygonMode polygonMode);
  PipelineBuilder WithPipelineLayout(vk::PipelineLayout pipelineLayout);
  PipelineBuilder WithScissors(int32_t xOffset, int32_t yOffset,
                               vk::Extent2D extent);
  PipelineBuilder
  WithDepthTestingSettings(bool doDepthTest, bool doDepthWrite,
                           vk::CompareOp compareOp = vk::CompareOp::eAlways);
  PipelineBuilder WithScissors(vk::Rect2D scissors);
  PipelineBuilder WithViewport(float_t x, float_t y, float_t width,
                               float_t height, float_t minDepth,
                               float_t maxDepth);
  PipelineBuilder WithViewport(vk::Viewport viewport);
  PipelineBuilder GetDefaultsForExtent(vk::Extent2D windowExtent);
  vk::Pipeline Build(vk::Device device, vk::RenderPass pass);
};
