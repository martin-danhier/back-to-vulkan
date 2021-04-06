//
// Created by Martin Danhier on 15/03/2021.
//

#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>
#include <VkBootstrap.h>
#include <fstream>
#include <glm/gtx/transform.hpp>
#include <iostream>
#include <string>

#include "vk_init.h"
#include "vk_types.h"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

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

  // Initialize meshes
  LoadMeshes();

  // Initialize scene
  InitScene();

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

  // Get required SDL extensions
  uint32_t sdlRequiredExtensionsCount = 0;
  if (!SDL_Vulkan_GetInstanceExtensions(_window, &sdlRequiredExtensionsCount,
                                   nullptr)) HandleSDLError();
  std::vector<const char *> sdlRequiredExtensions(sdlRequiredExtensionsCount);
  if (!SDL_Vulkan_GetInstanceExtensions(_window, &sdlRequiredExtensionsCount,
                                   sdlRequiredExtensions.data())) HandleSDLError();

  // Setup instance
  auto vkbInstanceBuilder = builder.set_app_name("Back to Vulkan")
                                .set_app_version(0, 1, 0)
                                .set_engine_name("MyEngine")
                                .set_engine_version(0, 1, 0)
                                .require_api_version(1, 1, 0);
  // Add sdl extensions
  for (const char *ext : sdlRequiredExtensions) {
    vkbInstanceBuilder.enable_extension(ext);
  }

  // Build instance
  auto vkbInstance = vkbInstanceBuilder.build().value();

  // Wrap the instance and debug messenger in vk-hpp handle classes and store
  // them
  _instance = vk::Instance(vkbInstance.instance);
  _debugMessenger = vk::DebugUtilsMessengerEXT(vkbInstance.debug_messenger);

  // Initialize function pointers for instance
  VULKAN_HPP_DEFAULT_DISPATCHER.init(_instance);

  // Get the surface of the SDL window
  VkSurfaceKHR surface;
  if (!SDL_Vulkan_CreateSurface(_window, _instance, &surface)) HandleSDLError();
 
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

  // Initialize memory allocator

  // Give VMA the functions pointers of vulkan functions
  // We need to do that since we load them dynamically
  VmaVulkanFunctions vulkanFunctions{
      VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceProperties,
      VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceMemoryProperties,
      VULKAN_HPP_DEFAULT_DISPATCHER.vkAllocateMemory,
      VULKAN_HPP_DEFAULT_DISPATCHER.vkFreeMemory,
      VULKAN_HPP_DEFAULT_DISPATCHER.vkMapMemory,
      VULKAN_HPP_DEFAULT_DISPATCHER.vkUnmapMemory,
      VULKAN_HPP_DEFAULT_DISPATCHER.vkFlushMappedMemoryRanges,
      VULKAN_HPP_DEFAULT_DISPATCHER.vkInvalidateMappedMemoryRanges,
      VULKAN_HPP_DEFAULT_DISPATCHER.vkBindBufferMemory,
      VULKAN_HPP_DEFAULT_DISPATCHER.vkBindImageMemory,
      VULKAN_HPP_DEFAULT_DISPATCHER.vkGetBufferMemoryRequirements,
      VULKAN_HPP_DEFAULT_DISPATCHER.vkGetImageMemoryRequirements,
      VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateBuffer,
      VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyBuffer,
      VULKAN_HPP_DEFAULT_DISPATCHER.vkCreateImage,
      VULKAN_HPP_DEFAULT_DISPATCHER.vkDestroyImage,
      VULKAN_HPP_DEFAULT_DISPATCHER.vkCmdCopyBuffer,
  };
  VmaAllocatorCreateInfo allocatorInfo{
      .physicalDevice = _chosenGPU,
      .device = _device,
      .pVulkanFunctions = &vulkanFunctions,
      .instance = _instance,
  };
  vmaCreateAllocator(&allocatorInfo, &_allocator);
}

void VulkanEngine::HandleSDLError() {
  std::cerr << "[SDL Error]\n" << SDL_GetError() << '\n';
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
  // Register deletion
  _mainDeletionQueue.PushFunction(
      [this]() { _device.destroySwapchainKHR(_swapchain); });

  //   Get image views
  _swapchainImageViews.resize(_swapchainImages.size());

  for (uint32_t i = 0; i < static_cast<uint32_t>(_swapchainImages.size()); i++) {
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

  // Create depth image
  vk::Extent3D depthImageExtent = {
      _windowExtent.width,
      _windowExtent.height,
      1,
  };
  _depthImageFormat = vk::Format::eD32Sfloat;

  // Allocate image
  auto imageCreateInfo = vkinit::ImageCreateInfo(
      _depthImageFormat, vk::ImageUsageFlagBits::eDepthStencilAttachment,
      depthImageExtent);
  VmaAllocationCreateInfo allocationCreateInfo{
      .usage = VMA_MEMORY_USAGE_GPU_ONLY,
      .requiredFlags = static_cast<VkMemoryPropertyFlags>(
          vk::MemoryPropertyFlagBits::eDeviceLocal),
  };
  vmaCreateImage(_allocator, (VkImageCreateInfo *)&imageCreateInfo,
                 &allocationCreateInfo, (VkImage *)&_depthImage.image,
                 &_depthImage.allocation, nullptr);

  // Create image view for the depth image to use for rendering
  auto imageViewCreateInfo = vkinit::ImageViewCreateInfo(
      _depthImageFormat, _depthImage.image, vk::ImageAspectFlagBits::eDepth);
  _depthImageView = _device.createImageView(imageViewCreateInfo);

  // Register deletion
  _mainDeletionQueue.PushFunction([this]() {
    _device.destroyImageView(_depthImageView);
    vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);
  });
}

void VulkanEngine::InitCommands() {
  // Create a command pool
  vk::CommandPoolCreateInfo commandPoolCreateInfo{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = _graphicsQueueFamily,
  };

  for (auto &frame : _frames) {
    frame.commandPool = _device.createCommandPool(commandPoolCreateInfo);

    // Create a primary command buffer
    vk::CommandBufferAllocateInfo commandBufferAllocateInfo{
        .commandPool = frame.commandPool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    };
    frame.mainCommandBuffer =
        _device.allocateCommandBuffers(commandBufferAllocateInfo)[0];
    // Register deletion
    _mainDeletionQueue.PushFunction(
        [this, frame]() { _device.destroyCommandPool(frame.commandPool); });
  }
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

  // Depth attachment
  vk::AttachmentDescription depthAttachment{
      // The attachment must use the same format as the swapchain
      .format = _depthImageFormat,
      // No multisampling, so 1 sample
      .samples = vk::SampleCountFlagBits::e1,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eStore,
      .stencilLoadOp = vk::AttachmentLoadOp::eClear,
      .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
      // Don't care about the starting layout of the attachment
      .initialLayout = vk::ImageLayout::eUndefined,
      // Once the renderpass ends, the image should be in a format ready for
      // presenting
      .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
  };

  // Reference to the attachment in the renderpass (sort of smart pointer)
  vk::AttachmentReference depthAttachmentRef{
      .attachment = 1, // Index of the colorAttachment in the renderpass
      .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
  };

  // Create the subpass
  vk::SubpassDescription subpass{
      .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachmentRef,
      .pDepthStencilAttachment = &depthAttachmentRef,
  };

  // Create the render pass
  vk::AttachmentDescription attachments[2] = {colorAttachment, depthAttachment};
  vk::RenderPassCreateInfo renderPassCreateInfo{
      .attachmentCount = 2,
      .pAttachments = attachments,
      .subpassCount = 1,
      .pSubpasses = &subpass,
  };
  _renderPass = _device.createRenderPass(renderPassCreateInfo);

  // Register deletion
  _mainDeletionQueue.PushFunction(
      [this]() { _device.destroyRenderPass(_renderPass); });
}

void VulkanEngine::InitFramebuffers() {
  // Create attachments
  vk::ImageView attachments[2];
  attachments[1] = _depthImageView;

  // Create the framebuffers
  vk::FramebufferCreateInfo framebufferCreateInfo{
      .renderPass = _renderPass,
      .attachmentCount = 2,
      .pAttachments = attachments,
      .width = _windowExtent.width,
      .height = _windowExtent.height,
      .layers = 1,
  };

  const uint32_t swapchainImageCount = _swapchainImages.size();

  // Init the framebuffers array
  _framebuffers = std::vector<vk::Framebuffer>(swapchainImageCount);
  for (uint32_t i = 0; i < swapchainImageCount; ++i) {

    // Link the corresponding image
    attachments[0] = _swapchainImageViews[i];
    // Create the framebuffer and store it in the array
    _framebuffers[i] = _device.createFramebuffer(framebufferCreateInfo);
    // Register deletion
    _mainDeletionQueue.PushFunction([this, i]() {
      _device.destroyFramebuffer(_framebuffers[i]);
      _device.destroyImageView(_swapchainImageViews[i]);
    });
  }
}

void VulkanEngine::InitSyncStructures() {
  // Create render fence
  vk::FenceCreateInfo fenceCreateInfo{
      .flags = vk::FenceCreateFlagBits::eSignaled,
  };
  vk::SemaphoreCreateInfo semaphoreCreateInfo{};

  // For each frame, create sync structures
  for (auto &frame : _frames) {
    // Create fence
    frame.renderFence = _device.createFence(fenceCreateInfo);

    // Register deletion
    _mainDeletionQueue.PushFunction(
        [this, frame]() { _device.destroyFence(frame.renderFence); });

    // Create semaphores
    frame.presentSemaphore = _device.createSemaphore(semaphoreCreateInfo);
    frame.renderSemaphore = _device.createSemaphore(semaphoreCreateInfo);

    // Register deletion
    _mainDeletionQueue.PushFunction([this, frame]() {
      _device.destroySemaphore(frame.presentSemaphore);
      _device.destroySemaphore(frame.renderSemaphore);
    });
  }
}

void VulkanEngine::InitPipelines() {

  // Load shaders
  auto coloredTriangleFragShader =
      LoadShaderModule("../shaders/colored_triangle.frag.spv");
  auto redTriangleFragShader = LoadShaderModule("../shaders/triangle.frag.spv");
  auto meshVertShader = LoadShaderModule("../shaders/tri_mesh.vert.spv");

  // Create pipelines

  VertexInputDescription vertexDescription = Vertex::GetVertexDescription();

  // Create the mesh pipeline layout
  auto meshPipelineLayoutCreateInfo = vkinit::PipelineLayoutCreateInfo();
  constexpr vk::PushConstantRange pushConstants{
      .stageFlags = vk::ShaderStageFlagBits::eVertex,
      .offset = 0,
      .size = sizeof(MeshPushConstants),
  };
  meshPipelineLayoutCreateInfo.pPushConstantRanges = &pushConstants;
  meshPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
  auto meshPipelineLayout =
      _device.createPipelineLayout(meshPipelineLayoutCreateInfo);

  // Create the mesh pipeline
  auto meshPipeline =
      PipelineBuilder()
          .WithPipelineLayout(meshPipelineLayout)
          .GetDefaultsForExtent(_windowExtent)
          .AddShaderStage(vk::ShaderStageFlagBits::eVertex, meshVertShader)
          .AddShaderStage(vk::ShaderStageFlagBits::eFragment,
                          coloredTriangleFragShader)
          .WithVertexInput(vertexDescription)
          .WithDepthTestingSettings(true, true, vk::CompareOp::eLessOrEqual)
          .Build(_device, _renderPass);

  // Create the red mesh pipeline
  auto redMeshPipeline =
      PipelineBuilder()
          .WithPipelineLayout(meshPipelineLayout)
          .GetDefaultsForExtent(_windowExtent)
          .AddShaderStage(vk::ShaderStageFlagBits::eVertex, meshVertShader)
          .AddShaderStage(vk::ShaderStageFlagBits::eFragment,
                          redTriangleFragShader)
          .WithVertexInput(vertexDescription)
          .WithDepthTestingSettings(true, true, vk::CompareOp::eLessOrEqual)
          .Build(_device, _renderPass);

  // Save materials
  CreateMaterial(meshPipeline, meshPipelineLayout, "default");
  CreateMaterial(redMeshPipeline, meshPipelineLayout, "red");

  // Destroy the shader modules as they are not needed anymore
  _device.destroyShaderModule(coloredTriangleFragShader);
  _device.destroyShaderModule(meshVertShader);
  _device.destroyShaderModule(redTriangleFragShader);

  // Register deletion
  _mainDeletionQueue.PushFunction([this, meshPipeline, redMeshPipeline, meshPipelineLayout]() {
    // Destroy pipelines
    _device.destroyPipeline(meshPipeline);
    _device.destroyPipeline(redMeshPipeline);

    // Destroy the layout
    _device.destroyPipelineLayout(meshPipelineLayout);
  });
}

vk::ShaderModule VulkanEngine::LoadShaderModule(const char *filePath) {
  // Load the binary file with the cursor at the end
  std::ifstream file(filePath, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    // Display error
    std::cerr << strerror(errno) << '\n';
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

void VulkanEngine::LoadMeshes() {
  // Make the array 3 vertices long
  std::vector<Vertex> triangleVertices(3);
  // Triangle
  triangleVertices[0].position = {1.f, 1.f, 0.f};
  triangleVertices[1].position = {-1.f, 1.f, 0.f};
  triangleVertices[2].position = {01.f, -1.f, 0.f};
  // Triangle colors, all green
  triangleVertices[0].color = {0.f, 1.0f, 0.f};
  triangleVertices[1].color = {0.f, 1.0f, 0.f};
  triangleVertices[2].color = {0.f, 1.0f, 0.f};

  _meshes["triangle"] = Mesh(triangleVertices);
  Mesh *triangleMesh = GetMesh("triangle");
  triangleMesh->Upload(_allocator, _mainDeletionQueue);

  // Load the monkey
  _meshes["monkey"] = Mesh();
  Mesh *monkeyMesh = GetMesh("monkey");
  monkeyMesh->LoadFromObj("../assets/monkey_smooth.obj");
  monkeyMesh->Upload(_allocator, _mainDeletionQueue);
}

void VulkanEngine::Cleanup() {
  if (_isInitialized) {

    // Wait for all fences until the GPU has stopped using the objects
    vk::Fence fences[FRAME_OVERLAP];
    for (uint32_t i = 0; i < FRAME_OVERLAP; i++) {
      fences[i] = _frames[i].renderFence;
    }
    _device.waitForFences(FRAME_OVERLAP, fences, true, 1000000000);

    _mainDeletionQueue.Flush();

    // Cleanup Vulkan
    vmaDestroyAllocator(_allocator);

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
  FrameData &currentFrame = GetCurrentFrame();

  auto waitResult = _device.waitForFences(currentFrame.renderFence, true, 1000000000);
  if (waitResult != vk::Result::eSuccess)
    throw std::runtime_error("Error while waiting for fences");
  _device.resetFences(currentFrame.renderFence);

  // Request image index from swapchain
  uint32_t swapchainImageIndex;
  auto nextImageResult =
      _device.acquireNextImageKHR(_swapchain, 1000000000, currentFrame.presentSemaphore, nullptr);
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
  currentFrame.mainCommandBuffer.reset({});

  // Begin the recording of commands into the buffer
  vk::CommandBufferBeginInfo cmdBeginInfo{
      .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
  };
  currentFrame.mainCommandBuffer.begin(cmdBeginInfo);

  // Define a clear color from frame number
  float flash = abs(sin(static_cast<float>(_frameNumber) / 120.f));
  float flash2 = abs(sin(static_cast<float>(_frameNumber) / 180.f));
  vk::ClearValue clearColorValue(vkinit::GetColor(1 - flash, flash2, flash));

  // Clear depth stencil
  vk::ClearValue clearDepthValue(vk::ClearDepthStencilValue{1.0f});
  vk::ClearValue clearValues[2] = {clearColorValue, clearDepthValue};

  // Start the main renderpass
  vk::RenderPassBeginInfo rpBeginInfo{
      .renderPass = _renderPass,
      .framebuffer = _framebuffers[swapchainImageIndex],
      .renderArea =
          {
              .offset = {.x = 0, .y = 0},
              .extent = _windowExtent,
          },
      .clearValueCount = 2,
      .pClearValues = clearValues,
  };
  currentFrame.mainCommandBuffer.beginRenderPass(rpBeginInfo, vk::SubpassContents::eInline);

  // ==== Start Render code ====

  // Draw objects
  DrawObjects(currentFrame.mainCommandBuffer, _renderables.data(), _renderables.size());

  // ==== End Render code ====

  // End the renderpass to finish rendering commands
  currentFrame.mainCommandBuffer.endRenderPass();
  // End the command buffer to finish it and prepare it to be submitted
  currentFrame.mainCommandBuffer.end();

  vk::PipelineStageFlags waitStage =
      vk::PipelineStageFlagBits::eColorAttachmentOutput;

  // Submit the buffer to the queue
  vk::SubmitInfo submitInfo{
      // Wait until the image to render to is ready
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &currentFrame.presentSemaphore,
      // Pipeline stage
      .pWaitDstStageMask = &waitStage,
      // Link the command buffer
      .commandBufferCount = 1,
      .pCommandBuffers = &currentFrame.mainCommandBuffer,
      // Signal the render semaphore
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &currentFrame.renderSemaphore,
  };
  _graphicsQueue.submit(submitInfo, currentFrame.renderFence);

  // Present the image on the screen
  vk::PresentInfoKHR presentInfo{
      // Wait until the rendering is complete
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &currentFrame.renderSemaphore,
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

  // Init variables for delta time computation
  uint64_t previousFrameTime = 0, currentFrameTime = 0;

  // Main loop
  while (!shouldQuit) {

    // Update delta time
    previousFrameTime = currentFrameTime;
    currentFrameTime = SDL_GetPerformanceCounter();
    _deltaTime = static_cast<double_t>(currentFrameTime - previousFrameTime) /
                 static_cast<double_t>(SDL_GetPerformanceFrequency());

    // Handle window event in a queue
    while (SDL_PollEvent(&event) != 0) {
      // Quit event: set shouldQuit to true to exit the main loop
      if (event.type == SDL_QUIT) {
        shouldQuit = true;
      }
      // Keypress event
      else if (event.type == SDL_KEYDOWN) {
        // Unit per second of the camera
        constexpr float_t CAMERA_MOVEMENT_SPEED = 2.5f;

        // Z
        if (event.key.keysym.sym == SDLK_z) {
          _cameraMotion.z = CAMERA_MOVEMENT_SPEED;
        }
        // S
        else if (event.key.keysym.sym == SDLK_s) {
          _cameraMotion.z = -CAMERA_MOVEMENT_SPEED;
        }
        // Q
        else if (event.key.keysym.sym == SDLK_q) {
          _cameraMotion.x = CAMERA_MOVEMENT_SPEED;
        }
        // D
        else if (event.key.keysym.sym == SDLK_d) {
          _cameraMotion.x = -CAMERA_MOVEMENT_SPEED;
        }
        // SPACE
        else if (event.key.keysym.sym == SDLK_SPACE) {
          _cameraMotion.y = -CAMERA_MOVEMENT_SPEED;
        }
        // SHIFT
        else if (event.key.keysym.sym == SDLK_LSHIFT) {
          _cameraMotion.y = CAMERA_MOVEMENT_SPEED;
        }
      }
      // Stop motion when releasing
      else if (event.type == SDL_KEYUP) {
        if (event.key.keysym.sym == SDLK_z || event.key.keysym.sym == SDLK_s) {
          _cameraMotion.z = 0.0f;
        } else if (event.key.keysym.sym == SDLK_q ||
                   event.key.keysym.sym == SDLK_d) {
          _cameraMotion.x = 0.0f;
        } else if (event.key.keysym.sym == SDLK_SPACE ||
                   event.key.keysym.sym == SDLK_LSHIFT) {
          _cameraMotion.y = 0.0f;
        }
      }
    }

    // Apply motions
    _cameraPosition += static_cast<float>(_deltaTime) * _cameraMotion;
    // Rotate monkey
    _renderables[0].transformMatrix =
        glm::rotate(_renderables[0].transformMatrix, glm::radians(1.0f),
                    glm::vec3(0.0f, 1.f, 0.f));

    // Run the rendering code
    Draw();
  }
}

Material *VulkanEngine::CreateMaterial(vk::Pipeline pipeline,
                                       vk::PipelineLayout layout,
                                       const std::string &name) {
  Material mat{
      .pipeline = pipeline,
      .pipelineLayout = layout,
  };
  // Save it
  _materials[name] = mat;
  return &_materials[name];
}

Material *VulkanEngine::GetMaterial(const std::string &name) {
  auto it = _materials.find(name);
  // Return nullptr if the material does not exist
  if (it == _materials.end()) {
    return nullptr;
  } else {
    return &it->second;
  }
}

Mesh *VulkanEngine::GetMesh(const std::string &name) {
  auto it = _meshes.find(name);
  // Return nullptr if the material does not exist
  if (it == _meshes.end()) {
    return nullptr;
  } else {
    return &it->second;
  }
}

void VulkanEngine::DrawObjects(vk::CommandBuffer cmd, RenderObject *first,
                               int32_t count) {

  // Camera position
  glm::mat4 view = glm::translate(glm::mat4(1.0f), _cameraPosition);
  // Camera projection
  glm::mat4 projection = glm::perspective(
      glm::radians(70.f),
      static_cast<float_t>(_windowExtent.width) / _windowExtent.height, 0.1f,
      200.0f);
  projection[1][1] *= -1;

  // Render objects
  Mesh *lastMesh = nullptr;
  Material *lastMaterial = nullptr;

  for (int i = 0; i < count; i++) {
    RenderObject &object = first[i];

    // Only bind pipeline if is it different from the already bound one
    if (object.material != lastMaterial) {
      cmd.bindPipeline(vk::PipelineBindPoint::eGraphics,
                       object.material->pipeline);
      lastMaterial = object.material;
    }

    glm::mat4 model = object.transformMatrix;

    // Compute render matrix
    glm::mat4 renderMatrix = projection * view * model;

    // Upload render matrix with push constants
    MeshPushConstants constants{
        .render_matrix = renderMatrix,
    };
    cmd.pushConstants(object.material->pipelineLayout,
                      vk::ShaderStageFlagBits::eVertex, 0,
                      sizeof(MeshPushConstants), &constants);

    // Only bind mesh if it is different from the already bound one
    if (object.mesh != lastMesh) {
      VkDeviceSize offset = 0;
      auto vertexBuffer = object.mesh->GetVertexBuffer();
      cmd.bindVertexBuffers(0, 1, &vertexBuffer, &offset);
      lastMesh = object.mesh;
    }

    // Draw
    cmd.draw(object.mesh->GetVertexCount(), 1, 0, 0);
  }
}
void VulkanEngine::InitScene() {
  // Set camera spawn point
  _cameraPosition = glm::vec3(3.f, 0.0f, 0.0f);

  // Create monkey render object
  Material *defaultMaterial = GetMaterial("default");
  RenderObject monkey{
      .mesh = GetMesh("monkey"),
      .material = defaultMaterial,
      .transformMatrix = glm::mat4{1.0f},
  };
  _renderables.push_back(monkey);

  Material *redMaterial = GetMaterial("red");
  RenderObject redMonkey{
      .mesh = GetMesh("monkey"),
      .material = redMaterial,
      .transformMatrix =
          glm::translate(glm::mat4{1.0f}, glm::vec3{3.0f, 0.f, 2.0f}),
  };
  _renderables.push_back(redMonkey);

  // Create triangles
  Mesh *triangleMesh = GetMesh("triangle");
  glm::mat4 scale = glm::scale(glm::mat4{1.0f}, glm::vec3(0.2, 0.2, 0.2));
  for (int x = -20; x <= 20; x++) {
    for (int y = -20; y <= 20; y++) {

      // Create transform matrix
      glm::mat4 translation =
          glm::translate(glm::mat4{1.0f}, glm::vec3(x, 0, y));

      // Create triangle render object
      RenderObject triangle{
          .mesh = triangleMesh,
          .material = defaultMaterial,
          .transformMatrix = translation * scale,
      };
      _renderables.push_back(triangle);
    }
  }
}
FrameData &VulkanEngine::GetCurrentFrame() {
  return _frames[_frameNumber % FRAME_OVERLAP];
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
    WithVertexInput(VertexInputDescription{
        .bindings = {},
        .attributes = {},
    });
  if (!_depthSettingsProvided)
    WithDepthTestingSettings(false, false);

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
      .pDepthStencilState = &_depthStencilCreateInfo,
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
PipelineBuilder PipelineBuilder::WithVertexInput(
    const VertexInputDescription &vertexInputDescription) {
  // Empty for now
  _vertexInputInfo = vk::PipelineVertexInputStateCreateInfo{
      .vertexBindingDescriptionCount =
          static_cast<uint32_t>(vertexInputDescription.bindings.size()),
      .pVertexBindingDescriptions = vertexInputDescription.bindings.data(),
      .vertexAttributeDescriptionCount =
          static_cast<uint32_t>(vertexInputDescription.attributes.size()),
      .pVertexAttributeDescriptions = vertexInputDescription.attributes.data(),
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
PipelineBuilder
PipelineBuilder::WithDepthTestingSettings(bool doDepthTest, bool doDepthWrite,
                                          vk::CompareOp compareOp) {

  _depthStencilCreateInfo = vk::PipelineDepthStencilStateCreateInfo{
      .depthTestEnable = doDepthTest,
      .depthWriteEnable = doDepthWrite,
      .depthCompareOp = doDepthTest ? compareOp : vk::CompareOp::eAlways,
      .depthBoundsTestEnable = false,
      .stencilTestEnable = false,
      .minDepthBounds = 0.0f,
      .maxDepthBounds = 1.0f,
  };

  _depthSettingsProvided = true;
  return *this;
}

// ==== DeletionQueue ====

void DeletionQueue::PushFunction(std::function<void()> &&ppFunction) {
  _deletors.push_back(ppFunction);
}
void DeletionQueue::Flush() {
  // Reverse iterate the deletion queue to execute all the functions
  for (auto it = _deletors.rbegin(); it != _deletors.rend(); it++) {
    (*it)();
  }

  _deletors.clear();
}
