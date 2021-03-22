//
// Created by Martin Danhier on 15/03/2021.
//

#include "vk_init.h"
std::array<float_t, 4> vkinit::GetColor(float_t r, float_t g, float_t b, float_t a) {
  return std::array<float_t, 4>{r, g, b, a};
}
