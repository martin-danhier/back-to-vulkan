//
// Created by Martin Danhier on 15/03/2021.
//

#pragma once

#include "vk_types.h"

namespace vkinit {
std::array<float, 4> GetColor(float r = 1.0f, float g = 1.0f,
                              float b = 1.0f, float a = 1.0f);

vk::PipelineLayoutCreateInfo PipelineLayoutCreateInfo();

}
