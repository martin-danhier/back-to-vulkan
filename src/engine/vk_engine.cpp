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

#ifndef NDEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void *pUserData) {

    // Get the message
    const std::string message(pCallbackData->pMessage);
    bool isError = false;

    // Create the formatted log
    // Preallocate the needed space to increase performance
    std::string formattedLog;
    formattedLog.reserve(26 + message.size());

    // Handle severity
     if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        formattedLog += "[VERBOSE | ";
    } else if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        formattedLog += "[INFO    | ";
    } else if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        formattedLog += "[WARNING | ";
    } else if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        formattedLog += "[ERROR   | ";
        isError = true;
    }

     // Handle type
    if (messageType <= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
        formattedLog += "GENERAL]      ";
    } else if (messageType <= VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
        formattedLog += "VALIDATION]   ";
    } else if (messageType <= VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
        formattedLog += "PERFORMANCE]  ";
    }

    // Add the message
    formattedLog += message;
    formattedLog += "\n";

    // Display the log in STDERR in case of error, display it in STDOUT otherwise
    if (isError) {
        std::cerr << formattedLog;
    } else {
        std::cout << formattedLog;
    }

    return VK_FALSE;
};
#endif

void VulkanEngine::InitVulkan() {

    // === Instance creation ===

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

    // Set required validation layers
#ifdef NDEBUG
    constexpr const char *requiredValidationLayers[] = nullptr;
    constexpr int requiredValidationLayersCount = 0;
#else
    constexpr const char *requiredValidationLayers[] = {
            "VK_LAYER_KHRONOS_validation"
    };
    constexpr int requiredValidationLayersCount = 1;

    // Check validation layer support
    const auto supportedLayers = vk::enumerateInstanceLayerProperties();

    // TODO refactor to use the same code as extensions
    for (const auto layer : requiredValidationLayers) {
        bool found = false;

        // Search for that layer in the supported layers
        for (int i = 0; i < supportedLayers.size() && !found; i++) {
            // Same name = found !
            if (strcmp(layer, supportedLayers[i].layerName) == 0) {
                found = true;
            }
        }

        // If it wasn't found: error
        if (!found) {
            auto message = "[ERROR] The \"" + std::string(layer) + "\" validation layer is not supported !";
            std::cerr << message << '\n';
            throw std::runtime_error(message);
        }
    }
#endif

    // Set required extensions
    std::vector<const char *> requiredExtensions = {
            VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME,
#ifndef NDEBUG
            // Add debug extensions
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
    };
    size_t additionalExtensionCount = requiredExtensions.size();

    // Get the number of SDL extensions
    uint32_t sdlExtensionCount = 0;
    SDL_Vulkan_GetInstanceExtensions(_window, &sdlExtensionCount, nullptr);

    // Resize the vector to be able to hold SDL extensions as well
    uint32_t totalExtensionCount = sdlExtensionCount + additionalExtensionCount;
    requiredExtensions.resize(totalExtensionCount);
    // Give to SDL a pointer to the extensions array + the index of the first
    // = we tell SDL to add its extensions at the end of the vector
    SDL_Vulkan_GetInstanceExtensions(_window, &sdlExtensionCount,
                                     requiredExtensions.data() + additionalExtensionCount);

    // Get supported extensions
    const auto supportedExtensions = vk::enumerateInstanceExtensionProperties();

    // Check if the required extensions are supported
    for (const auto extension: requiredExtensions) {
        bool found = false;

        for (int i = 0; i < supportedExtensions.size() && !found; i++) {

            // If same name : found !
            if (strcmp(extension, supportedExtensions[i].extensionName) == 0) {
                found = true;
            }
        }
        // If it wasn't found: error
        if (!found) {
            auto message = "[ERROR] The \"" + std::string(extension) + "\" extension is not supported !";
            std::cerr << message << '\n';
            throw std::runtime_error(message);
        }
    }

    // Create create infos
    const vk::InstanceCreateInfo instanceCreateInfo{
            .pApplicationInfo           = &applicationInfo,
            // Validation layers
            .enabledLayerCount          = requiredValidationLayersCount,
            .ppEnabledLayerNames        = requiredValidationLayers,
            // Extensions
            .enabledExtensionCount      = totalExtensionCount,
            .ppEnabledExtensionNames    = requiredExtensions.data(),
    };

    // Create instance
    _instance = vk::createInstance(instanceCreateInfo);

    // Initialize function pointers for instance
    VULKAN_HPP_DEFAULT_DISPATCHER.init(_instance);

    // === Debug messenger ===

#ifndef NDEBUG
    vk::DebugUtilsMessengerCreateInfoEXT messengerCreateInfo{
            // Define allowed log severity
            .messageSeverity    = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                                  vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                  vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose,
            // Define allowed message types
            .messageType        = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                  vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                  vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
            // Define callback
            .pfnUserCallback    = debugCallback,
            .pUserData          = nullptr,
    };
    // Create debug messenger
    _debugMessenger = _instance.createDebugUtilsMessengerEXT(messengerCreateInfo);
#endif


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
#ifndef NDEBUG
        _instance.destroyDebugUtilsMessengerEXT(_debugMessenger);
#endif
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




