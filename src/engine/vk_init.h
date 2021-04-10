//
// Created by Martin Danhier on 15/03/2021.
//

#pragma once

#include "vk_types.h"

class DeletionQueue;

namespace vkinit {
std::array<float, 4> GetColor(float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f);

vk::PipelineLayoutCreateInfo PipelineLayoutCreateInfo();
vk::ImageCreateInfo ImageCreateInfo(vk::Format format, vk::ImageUsageFlags flags, vk::Extent3D extent);
vk::ImageViewCreateInfo ImageViewCreateInfo(vk::Format format, vk::Image image,
                                            vk::ImageAspectFlags aspectFlags);

// Layout builder
struct DescriptorSetLayout {
  std::vector<vk::DescriptorSetLayoutBinding> bindings;
  vk::DescriptorSetLayout layout;
};

class DescriptorSetLayoutBuilder {

private:
  std::vector<vk::DescriptorSetLayoutBinding> _bindings;

public:
  DescriptorSetLayoutBuilder AddBinding(vk::ShaderStageFlags stages, vk::DescriptorType descriptorType);
  DescriptorSetLayout Build(const vk::Device &device, DeletionQueue &deletionQueue);
};

// Set writer
class DescriptorSetWriter {
private:
  std::vector<vk::WriteDescriptorSet> _writes;
  std::vector<vk::DescriptorBufferInfo> _bufferInfos;
  std::vector<std::vector<vk::DescriptorSetLayoutBinding>> _bindings;
  std::vector<vk::DescriptorSet *> _sets;

public:
  DescriptorSetWriter(const std::vector<vk::DescriptorSet *> &sets,
                      const std::vector<std::vector<vk::DescriptorSetLayoutBinding>> &bindings);
  DescriptorSetWriter AddBuffer(uint32_t setIndex, uint32_t bindingIndex, vk::Buffer buffer, size_t range,
                                size_t offset = 0);
  void Write(const vk::Device &device);
};

// Set allocator
class DescriptorSetAllocator {
private:
  vk::DescriptorPool _pool;
  std::vector<vk::DescriptorSet *> _sets;
  std::vector<vk::DescriptorSetLayout> _layouts;
  std::vector<std::vector<vk::DescriptorSetLayoutBinding>> _bindings;

public:
  explicit DescriptorSetAllocator(vk::DescriptorPool pool);
  DescriptorSetAllocator AddSetWithLayout(const DescriptorSetLayout &layout, vk::DescriptorSet *dstSet);
  vkinit::DescriptorSetWriter Allocate(const vk::Device &device);
};

} // namespace vkinit
