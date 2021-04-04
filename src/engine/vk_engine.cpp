//
// Created by Martin Danhier on 15/03/2021.
//

#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <VkBootstrap.h>
#include <fstream>
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

  // Initialize pipelines
  InitPipelines();

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

void VulkanEngine::InitPipelines() {

  // Load shaders
  auto triangleFragShader = LoadShaderModule("../shaders/triangle.frag.spv");
  auto triangleVertShader = LoadShaderModule("../shaders/triangle.vert.spv");

  // Create the trianglePipeline layout
  auto pipelineLayoutCreateInfo = vkinit::PipelineLayoutCreateInfo();
  _trianglePipelineLayout =
      _device.createPipelineLayout(pipelineLayoutCreateInfo);

  // Create the trianglePipeline
  _trianglePipeline =
      PipelineBuilder()
          .WithPipelineLayout(_trianglePipelineLayout)
          // Register shaders
          .AddShaderStage(vk::ShaderStageFlagBits::eVertex, triangleVertShader)
          .AddShaderStage(vk::ShaderStageFlagBits::eFragment,
                          triangleFragShader)
          .GetDefaultsForExtent(_windowExtent)
          .Build(_device, _renderPass);
}

vk::ShaderModule VulkanEngine::LoadShaderModule(const char *filePath) {
  // Load the binary file with the cursor at the end
  std::ifstream file(filePath, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("Couldn't load shader " + std::string(filePath));
  }
  // Since the cursor is at the end, tellg gives the size of the file
  size_t fileSize = file.tellg();
  // Create a vector long enough to hold the content
  std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
  // Cursor at the beginning
  file.seekg(0);
  // Load the file into the buffer
  file.read((char *)buffer.data(), fileSize);
  // Close the file
  file.close();

  // Create a shader module
  vk::ShaderModuleCreateInfo shaderCreateInfo{
      .codeSize = buffer.size() * sizeof(uint32_t),
      .pCode = buffer.data(),
  };
  return _device.createShaderModule(shaderCreateInfo);
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
  auto nextImageResult =
      _device.acquireNextImageKHR(_swapchain, 1000000000, _presentSemaphore);
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
  float flash = abs(sin(static_cast<float>(_frameNumber) / 120.f));
  float flash2 = abs(sin(static_cast<float>(_frameNumber) / 180.f));
  vk::ClearValue clearValue(vkinit::GetColor(1 - flash, flash2, flash));

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

  _mainCommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                  _trianglePipeline);
  _mainCommandBuffer.draw(3, 1, 0, 0);

  // End the renderpass to finish rendering commands
  _mainCommandBuffer.endRenderPass();
  // End the command buffer to finish it and prepare it to be submitted
  _mainCommandBuffer.end();

  vk::PipelineStageFlags waitStage =
      vk::PipelineStageFlagBits::eColorAttachmentOutput;

  // Submit the buffer to the queue
  vk::SubmitInfo submitInfo{
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
  vk::PresentInfoKHR presentInfo{
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

// ===== PIPELINE BUILDER =====

vk::Pipeline PipelineBuilder::Build(vk::Device device, vk::RenderPass pass) {
  // Set pipeline blend
  _colorBlendAttachment = vk::PipelineColorBlendAttachmentState{
      .blendEnable = false,
      .colorWriteMask =
          vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
          vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
  };

  // Set pipeline layout to default
  _multisampling = vk::PipelineMultisampleStateCreateInfo{
      .rasterizationSamples = vk::SampleCountFlagBits::e1,
      .sampleShadingEnable = false,
      .minSampleShading = 1.0f,
      .pSampleMask = nullptr,
      .alphaToCoverageEnable = false,
      .alphaToOneEnable = false,
  };

  // Apply defaults if needed
  if (!_rasterizerInited)
    WithPolygonMode(vk::PolygonMode::eFill);
  if (!_inputAssemblyInited)
    WithAssemblyTopology(vk::PrimitiveTopology::eTriangleList);
  if (!_vertexInputInited)
    WithVertexInput();

  // Create viewport state from stored viewport and scissors
  vk::PipelineViewportStateCreateInfo viewportState{
      .viewportCount = 1,
      .pViewports = &_viewport,
      .scissorCount = 1,
      .pScissors = &_scissor,
  };
  // Create color blending state
  vk::PipelineColorBlendStateCreateInfo colorBlending{
      .logicOpEnable = false,
      .logicOp = vk::LogicOp::eCopy,
      .attachmentCount = 1,
      .pAttachments = &_colorBlendAttachment,
  };

#ifndef NDEBUG
  if (!_pipelineLayoutInited) {
    throw std::runtime_error(
        "Pipeline layout must be given to the pipeline builder.");
  }
  if (!_viewportInited) {
    throw std::runtime_error("Viewport must be given to the pipeline builder.");
  }
  if (!_scissorsInited) {
    throw std::runtime_error("Scissors must be given to the pipeline builder.");
  }
#endif

  // Create the pipeline
  vk::GraphicsPipelineCreateInfo pipelineCreateInfo{
      .stageCount = static_cast<uint32_t>(_shaderStages.size()),
      .pStages = _shaderStages.data(),
      .pVertexInputState = &_vertexInputInfo,
      .pInputAssemblyState = &_inputAssembly,
      .pViewportState = &viewportState,
      .pRasterizationState = &_rasterizer,
      .pMultisampleState = &_multisampling,
      .pColorBlendState = &colorBlending,
      .layout = _pipelineLayout,
      .renderPass = pass,
      .subpass = 0,
      .basePipelineHandle = nullptr,
  };
  auto result = device.createGraphicsPipeline(nullptr, pipelineCreateInfo);
  // Handle result
  switch (result.result) {
  case vk::Result::eSuccess:
    return result.value;
  // Default returns an exception
  default:
    throw std::runtime_error("Failed to create Pipeline");
  }
}

PipelineBuilder PipelineBuilder::AddShaderStage(vk::ShaderStageFlagBits stage,
                                                vk::ShaderModule shaderModule) {

  // Add a new shader stage to the vector
  _shaderStages.push_back(vk::PipelineShaderStageCreateInfo{
      .stage = stage,
      .module = shaderModule,
      // Entry function of the shader, we use main conventionally
      .pName = "main",
  });

  // Return this so it is easier to chain functions
  return *this;
}
PipelineBuilder PipelineBuilder::WithVertexInput() {

  // Empty for now
  _vertexInputInfo = vk::PipelineVertexInputStateCreateInfo{
      .vertexBindingDescriptionCount = 0,
      .vertexAttributeDescriptionCount = 0,
  };
  _vertexInputInited = true;

  return *this;
}
PipelineBuilder
PipelineBuilder::WithAssemblyTopology(vk::PrimitiveTopology topology) {
  _inputAssembly = vk::PipelineInputAssemblyStateCreateInfo{
      .topology = topology,
      .primitiveRestartEnable = false,
  };
  _inputAssemblyInited = true;

  return *this;
}
PipelineBuilder PipelineBuilder::WithPolygonMode(vk::PolygonMode polygonMode) {
  _rasterizer = vk::PipelineRasterizationStateCreateInfo{
      .depthClampEnable = false,
      // Keep the primitive in the rasterization stage
      .rasterizerDiscardEnable = false,
      .polygonMode = polygonMode,
      // No backface culling
      .cullMode = vk::CullModeFlagBits::eNone,
      .frontFace = vk::FrontFace::eCounterClockwise,
      // No depth bias
      .depthBiasEnable = false,
      .depthBiasConstantFactor = 0.0f,
      .depthBiasClamp = 0.0f,
      .depthBiasSlopeFactor = 0.0f,
      // Width of the line
      .lineWidth = 1.0f,
  };
  _rasterizerInited = true;

  return *this;
}
PipelineBuilder
PipelineBuilder::WithPipelineLayout(vk::PipelineLayout pipelineLayout) {
  _pipelineLayout = pipelineLayout;
#ifndef NDEBUG
  _pipelineLayoutInited = true;
#endif

  return *this;
}

// Scissors

PipelineBuilder PipelineBuilder::WithScissors(int32_t xOffset, int32_t yOffset,
                                              vk::Extent2D extent) {
  _scissor = vk::Rect2D{
      .offset{
          .x = xOffset,
          .y = yOffset,
      },
      .extent = extent,
  };
#ifndef NDEBUG
  _scissorsInited = true;
#endif
  return *this;
}

PipelineBuilder PipelineBuilder::WithScissors(vk::Rect2D scissors) {
  _scissor = scissors;
#ifndef NDEBUG
  _scissorsInited = true;
#endif
  return *this;
}

// Viewport

PipelineBuilder PipelineBuilder::WithViewport(float x, float y,
                                              float width, float height,
                                              float minDepth,
                                              float maxDepth) {

  _viewport = vk::Viewport{
      .x = x,
      .y = y,
      .width = width,
      .height = height,
      .minDepth = minDepth,
      .maxDepth = maxDepth,
  };
#ifndef NDEBUG
  _viewportInited = true;
#endif
  return *this;
}

PipelineBuilder PipelineBuilder::WithViewport(vk::Viewport viewport) {
  _viewport = viewport;
#ifndef NDEBUG
  _viewportInited = true;
#endif
  return *this;
}
// Defaults
PipelineBuilder
PipelineBuilder::GetDefaultsForExtent(vk::Extent2D windowExtent) {
  // Setup a viewport that takes the whole screen
  WithViewport(0.0f, 0.0f, windowExtent.width, windowExtent.height, 0.0f, 1.0f);
  WithScissors(0, 0, windowExtent);

  return *this;
}
