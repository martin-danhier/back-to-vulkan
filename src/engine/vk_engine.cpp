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
#include <glm/gtx/transform.hpp>

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
      [=]() { _device.destroySwapchainKHR(_swapchain); });

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
  // Register deletion
  _mainDeletionQueue.PushFunction(
      [=]() { _device.destroyCommandPool(_commandPool); });
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

  // Register deletion
  _mainDeletionQueue.PushFunction(
      [=]() { _device.destroyRenderPass(_renderPass); });
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
    // Register deletion
    _mainDeletionQueue.PushFunction([=]() {
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
  _renderFence = _device.createFence(fenceCreateInfo);
  // Register deletion
  _mainDeletionQueue.PushFunction(
      [=]() { _device.destroyFence(_renderFence); });

  // Create semaphores
  _presentSemaphore = _device.createSemaphore({});
  _renderSemaphore = _device.createSemaphore({});
  // Register deletion
  _mainDeletionQueue.PushFunction([=]() {
    _device.destroySemaphore(_presentSemaphore);
    _device.destroySemaphore(_renderSemaphore);
  });
}

void VulkanEngine::InitPipelines() {

  // Load shaders
  auto triangleFragShader = LoadShaderModule("../shaders/triangle.frag.spv");
  auto triangleVertShader = LoadShaderModule("../shaders/triangle.vert.spv");
  auto coloredTriangleFragShader =
      LoadShaderModule("../shaders/colored_triangle.frag.spv");
  auto coloredTriangleVertShader =
      LoadShaderModule("../shaders/colored_triangle.vert.spv");
  auto meshVertShader = LoadShaderModule("../shaders/tri_mesh.vert.spv");

  // Create the mesh pipeline layout
  VertexInputDescription vertexDescription = Vertex::GetVertexDescription();

  auto meshPipelineLayoutCreateInfo = vkinit::PipelineLayoutCreateInfo();
  constexpr vk::PushConstantRange pushConstants {
      .stageFlags = vk::ShaderStageFlagBits::eVertex,
      .offset = 0,
      .size = sizeof(MeshPushConstants),
  };
  meshPipelineLayoutCreateInfo.pPushConstantRanges = &pushConstants;
  meshPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
  _meshPipelineLayout = _device.createPipelineLayout(meshPipelineLayoutCreateInfo);


  // Create the mesh pipeline
  _meshPipeline =
      PipelineBuilder()
          .WithPipelineLayout(_meshPipelineLayout)
          .GetDefaultsForExtent(_windowExtent)
          .AddShaderStage(vk::ShaderStageFlagBits::eVertex, meshVertShader)
          .AddShaderStage(vk::ShaderStageFlagBits::eFragment,
                          coloredTriangleFragShader)
          .WithVertexInput(vertexDescription)
          .Build(_device, _renderPass);

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

  // Create the colored pipeline
  _coloredTrianglePipeline =
      PipelineBuilder()
          .WithPipelineLayout(_trianglePipelineLayout)
          .AddShaderStage(vk::ShaderStageFlagBits::eVertex,
                          coloredTriangleVertShader)
          .AddShaderStage(vk::ShaderStageFlagBits::eFragment,
                          coloredTriangleFragShader)
          .GetDefaultsForExtent(_windowExtent)
          .Build(_device, _renderPass);

  // Destroy the shader modules as they are not needed anymore
  _device.destroyShaderModule(triangleFragShader);
  _device.destroyShaderModule(triangleVertShader);
  _device.destroyShaderModule(coloredTriangleFragShader);
  _device.destroyShaderModule(coloredTriangleVertShader);
  _device.destroyShaderModule(meshVertShader);

  // Register deletion
  _mainDeletionQueue.PushFunction([=]() {
    // Destroy pipelines
    _device.destroyPipeline(_trianglePipeline);
    _device.destroyPipeline(_coloredTrianglePipeline);
    _device.destroyPipeline(_meshPipeline);

    // Destroy the layout
    _device.destroyPipelineLayout(_meshPipelineLayout);
    _device.destroyPipelineLayout(_trianglePipelineLayout);
  });
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
  _triangleMesh = Mesh(triangleVertices);

  // Load the monkey
  _monkeyMesh.LoadFromObj("../assets/monkey_smooth.obj");

  // Upload meshes
  _triangleMesh.Upload(_allocator, _mainDeletionQueue);
  _monkeyMesh.Upload(_allocator, _mainDeletionQueue);
}

void VulkanEngine::Cleanup() {
  if (_isInitialized) {

    // Wait until the GPU has stopped using the objects
    _device.waitForFences(1, &_renderFence, true, 1000000000);

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
  float flash = abs(sin(static_cast<float_t>(_frameNumber) / 120.f));
  float flash2 = abs(sin(static_cast<float_t>(_frameNumber) / 180.f));
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

  // ==== Start Render code ====

  if (_selectedShader == 0) {
    _mainCommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                    _trianglePipeline);
    _mainCommandBuffer.draw(3, 1, 0, 0);

  } else if (_selectedShader == 1) {
    _mainCommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                    _coloredTrianglePipeline);
    _mainCommandBuffer.draw(3, 1, 0, 0);
  } else {
    _mainCommandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                    _meshPipeline);
    Mesh selectedMesh;
    if (_selectedShader == 2) selectedMesh = _triangleMesh;
    else selectedMesh = _monkeyMesh;

    VkDeviceSize offset = 0;
    vk::Buffer buffer = selectedMesh.GetVertexBuffer();
    _mainCommandBuffer.bindVertexBuffers(0, 1, &buffer, &offset);

    //make a model view matrix for rendering the object
    //camera position
    glm::vec3 camPos = { 0.f,0.f,-2.f };

    glm::mat4 view = glm::translate(glm::mat4(1.f), camPos);
    //camera projection
    glm::mat4 projection = glm::perspective(glm::radians(70.f), 1700.f / 900.f, 0.1f, 200.0f);
    projection[1][1] *= -1;
    //model rotation
    glm::mat4 model = glm::rotate(glm::mat4{ 1.0f }, glm::radians(static_cast<float_t>(_frameNumber) * 0.4f), glm::vec3(0, 1, 0));

    //calculate final mesh matrix
    glm::mat4 mesh_matrix = projection * view * model;

    MeshPushConstants constants {
        .render_matrix = mesh_matrix,
    };
    _mainCommandBuffer.pushConstants(_meshPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(MeshPushConstants), &constants);

    // Draw
    _mainCommandBuffer.draw(selectedMesh.GetVertexCount(), 1, 0, 0);
  }



  // ==== End Render code ====

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
        // If that key is space, swap the shaders
        if (event.key.keysym.sym == SDLK_SPACE) {
          _selectedShader = (_selectedShader + 1) % 4;
        }
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
    WithVertexInput(VertexInputDescription{
        .bindings = {},
        .attributes = {},
    });

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

PipelineBuilder PipelineBuilder::WithViewport(float_t x, float_t y,
                                              float_t width, float_t height,
                                              float_t minDepth,
                                              float_t maxDepth) {

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
