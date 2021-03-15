//
// Created by Martin Danhier on 15/03/2021.
//

#pragma once

#include "vk_types.h"

class VulkanEngine {
private:

    bool _isInitialized{false};
    int _frameNumber{0};

    VkExtent2D _windowExtent{1700, 900};

    struct SDL_Window *_window{nullptr};

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

