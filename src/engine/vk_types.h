//
// Created by Martin Danhier on 15/03/2021.
//

#pragma once

// Include vulkan
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

struct AllocatedBuffer {
  vk::Buffer buffer;
  VmaAllocation allocation{};
};