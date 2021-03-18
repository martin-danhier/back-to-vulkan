//
// Created by Martin Danhier on 15/03/2021.
//

#include "vk_engine.h"

#include <iostream>
#include <SDL.h>
#include <string>
#include <SDL_vulkan.h>

#include "vk_types.h"
#include "vk_init.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

void VulkanEngine::Init() {
    // Initialize SDL
    SDL_Init(SDL_INIT_VIDEO);

    // Create a window
    SDL_WindowFlags windowFlags = SDL_WINDOW_VULKAN;
    _window = SDL_CreateWindow(
            "Back to Vulkan !",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            _windowExtent.width,
            _windowExtent.height,
            windowFlags
    );

    // Load the core Vulkan structures
    InitVulkan();

    // Initialize swapchain
//    InitSwapchain();

    // Set the _isInitialized property to true if everything went fine
    _isInitialized = true;
}

void VulkanEngine::InitVulkan() {

    // Init dynamic loader
    vk::DynamicLoader loader;
    auto vkGetInstanceProcAddr = loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
    VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

    // Create application info
    vk::ApplicationInfo applicationInfo{
            .pApplicationName   = "Back to Vulkan",
            .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
            .pEngineName        = "MyEngine",
            .engineVersion      = VK_MAKE_VERSION(0, 1, 0),
            .apiVersion         = VK_API_VERSION_1_1,
    };

    // Get the SDL required extensions

    // Set required extensions
    std::vector<const char *> extensionNames = {
            VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME,
    };
    size_t additionalExtensionCount = extensionNames.size();

    // Get the number of SDL extensions
    uint32_t sdlExtensionCount = 0;
    SDL_Vulkan_GetInstanceExtensions(_window, &sdlExtensionCount, nullptr);

    // Resize the vector to be able to hold SDL extensions as well
    uint32_t totalExtensionCount = sdlExtensionCount + additionalExtensionCount;
    extensionNames.resize(totalExtensionCount);
    // Give to SDL a pointer to the extensions array + the index of the first
    // = we tell SDL to add its extensions at the end of the vector
    SDL_Vulkan_GetInstanceExtensions(_window, &sdlExtensionCount,
                                     extensionNames.data() + additionalExtensionCount);

    // Create create infos
    const vk::InstanceCreateInfo createInfo{
            .pApplicationInfo           = &applicationInfo,
            // Validation layers
            .enabledLayerCount          = 0,
            .ppEnabledLayerNames        = nullptr,
            // Extensions
            .enabledExtensionCount      = totalExtensionCount,
            .ppEnabledExtensionNames    = extensionNames.data(),
    };

    // Create instance
    _instance = vk::createInstance(createInfo);

    // Initialize function pointers for instance
    VULKAN_HPP_DEFAULT_DISPATCHER.init(_instance);




    // Get the surface of the SDL window
//    SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

    // Select a GPU
    // We want a GPU that can write to the SDL surface and supports Vulkan 1.1
//    vkb::PhysicalDeviceSelector selector{vkbInstance};
//    auto vkbPhysicalDevice = selector
//            .set_minimum_version(1, 1)
//            .set_surface(_surface)
//            .select()
//            .value();
//
//    // Get logical device
//    vkb::DeviceBuilder deviceBuilder{vkbPhysicalDevice};
//    auto vkbDevice = deviceBuilder.build().value();

    // Save devices
//    _device = vkbDevice.device;
//    _chosenGPU = vkbPhysicalDevice.physical_device;
}

void VulkanEngine::InitSwapchain() {
//    // Initialize swapchaine with vkb
//    vkb::SwapchainBuilder swapchainBuilder{_chosenGPU, _device, _surface};
//    VkSurfaceFormatKHR format {
//            VK_FORMAT_B8G8R8A8_UNORM,
//            VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
//    };
//    auto vkbSwapchain = swapchainBuilder
//            .set_desired_format(format)
//            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
//            .set_desired_extent(_windowExtent.width, _windowExtent.height)
//            .build()
//            .value();
//
//    // Store swapchain and images
//    _swapchain = vkbSwapchain.swapchain;
//    _swapchainImages = vkbSwapchain.get_images().value();
//    _swapchainImageViews = vkbSwapchain.get_image_views().value();
//    _swapchainImageFormat = vkbSwapchain.image_format;
}

void VulkanEngine::Cleanup() {
    if (_isInitialized) {

        // Destroy everything in the opposite order as the creation order

        // Destroy swapchain
//        vkDestroySwapchainKHR(_device, _swapchain, nullptr);

        // Destroy swapchain image views
//        for (auto imageView : _swapchainImageViews) {
//            vkDestroyImageView(_device, imageView, nullptr);
//        }

        // Cleanup Vulkan
//        vkDestroyDevice(_device, nullptr);
////        vkDestroySurfaceKHR(_instance, _surface, nullptr);
////        vkb::destroy_debug_utils_messenger(_instance, _debugMessenger);
        _instance.destroy();
//
//        // Destroy SDL window
//        SDL_DestroyWindow(_window);
    }
}

void VulkanEngine::Draw() {

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




