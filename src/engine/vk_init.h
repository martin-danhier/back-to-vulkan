//
// Created by Martin Danhier on 15/03/2021.
//

#pragma once

#include "vk_types.h"

class DeletionQueue;

namespace vkinit {
std::array<float, 4> GetColor(float r = 1.0f, float g = 1.0f, float b = 1.0f,
                              float a = 1.0f);

vk::PipelineLayoutCreateInfo PipelineLayoutCreateInfo();
vk::ImageCreateInfo ImageCreateInfo(vk::Format format,
                                    vk::ImageUsageFlags flags,
                                    vk::Extent3D extent);
vk::ImageViewCreateInfo ImageViewCreateInfo(vk::Format format, vk::Image image,
                                            vk::ImageAspectFlags aspectFlags);


class DescriptorSetBuilder {
  private:
  vk::DescriptorPool _pool;
  vk::Device _device;
  DeletionQueue _deletionQueue;
  std::vector<vk::DescriptorSet*> _sets;
  std::vector<vk::DescriptorSetLayout> _layouts;
  std::vector<vk::DescriptorSetLayoutBinding> _bindings;
  std::vector<vk::WriteDescriptorSet> _writes;
  std::vector<size_t> _bindingCounts;

  public:
  DescriptorSetBuilder(vk::DescriptorPool &pool, vk::Device &device, DeletionQueue &deletionQueue);
  DescriptorSetBuilder SaveDescriptorSet(vk::DescriptorSetLayout *layout, vk::DescriptorSet *set);
  DescriptorSetBuilder AddBuffer(vk::ShaderStageFlags stages, vk::DescriptorType descriptorType, const vk::Buffer &buffer, size_t range, size_t offset = 0);
  void Build();
};

} // namespace vkinit

