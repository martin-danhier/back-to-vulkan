//
// Created by Martin Danhier on 15/03/2021.
//

#include "vk_init.h"

std::array<float, 4> vkinit::GetColor(float r, float g, float b,
                                        float a) {
  return std::array<float, 4>{r, g, b, a};
}

vk::PipelineLayoutCreateInfo vkinit::PipelineLayoutCreateInfo() {
  vk::PipelineLayoutCreateInfo info{
      .flags = {},
      .setLayoutCount = 0,
      .pSetLayouts = nullptr,
      .pushConstantRangeCount = 0,
      .pPushConstantRanges = nullptr,
  };
  return info;
}
vk::ImageViewCreateInfo
vkinit::ImageViewCreateInfo(vk::Format format, vk::Image image,
                            vk::ImageAspectFlags aspectFlags) {
  return vk::ImageViewCreateInfo{
      .image = image,
      .viewType = vk::ImageViewType::e2D,
      .format = format,
      .subresourceRange{
          .aspectMask = aspectFlags,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1,
      },
  };
}

vk::ImageCreateInfo vkinit::ImageCreateInfo(vk::Format format,
                                    vk::ImageUsageFlags flags,
                                    vk::Extent3D extent) {
  return vk::ImageCreateInfo{
      .imageType = vk::ImageType::e2D,
      .format = format,
      .extent = extent,
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = vk::SampleCountFlagBits::e1,
      .tiling = vk::ImageTiling::eOptimal,
      .usage = flags,
  };
}