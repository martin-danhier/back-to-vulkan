//
// Created by Martin Danhier on 15/03/2021.
//

#pragma once

#include "vk_types.h"

namespace vkinit {
std::array<float_t, 4> GetColor(float_t r = 1.0f, float_t g = 1.0f,
                              float_t b = 1.0f, float_t a = 1.0f);

vk::PipelineLayoutCreateInfo PipelineLayoutCreateInfo();
vk::ImageCreateInfo ImageCreateInfo(vk::Format format, vk::ImageUsageFlags flags, vk::Extent3D extent);
vk::ImageViewCreateInfo ImageViewCreateInfo(vk::Format format, vk::Image image, vk::ImageAspectFlags aspectFlags);

}
