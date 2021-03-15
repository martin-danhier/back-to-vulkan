//
// Created by Martin Danhier on 15/03/2021.
//

#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include "vk_types.h"
#include "vk_init.h"

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

    // Set the _isInitialized property to true if everything went fine
    _isInitialized = true;
}

void VulkanEngine::Cleanup() {
    if (_isInitialized) {
        // Destroy SDL window
        SDL_DestroyWindow(_window);
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
        }

        // Run the rendering code
        Draw();
    }
}
