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
