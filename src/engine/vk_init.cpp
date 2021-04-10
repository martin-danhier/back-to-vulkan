//
// Created by Martin Danhier on 15/03/2021.
//

#include "vk_init.h"
#include "vk_engine.h"
#include <iostream>

std::array<float, 4> vkinit::GetColor(float r, float g, float b, float a) {
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
vk::ImageViewCreateInfo vkinit::ImageViewCreateInfo(vk::Format format, vk::Image image,
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

vk::ImageCreateInfo vkinit::ImageCreateInfo(vk::Format format, vk::ImageUsageFlags flags,
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

// ==== Set allocator ===

vkinit::DescriptorSetAllocator::DescriptorSetAllocator(vk::DescriptorPool pool) { _pool = pool; }

vkinit::DescriptorSetWriter vkinit::DescriptorSetAllocator::Allocate(const vk::Device &device) {
  // Allocate sets
  vk::DescriptorSetAllocateInfo setAllocInfo{
      .descriptorPool = _pool,
      .descriptorSetCount = static_cast<uint32_t>(_layouts.size()),
      .pSetLayouts = _layouts.data(),
  };
  auto sets = device.allocateDescriptorSets(setAllocInfo);
  // Save sets
  for (uint32_t i = 0; i < sets.size(); i++) {
    *_sets[i] = sets[i];
  }

  return vkinit::DescriptorSetWriter(_sets, _bindings);
}

vkinit::DescriptorSetAllocator
vkinit::DescriptorSetAllocator::AddSetWithLayout(const vkinit::DescriptorSetLayout &layout,
                                                 vk::DescriptorSet *dstSet) {
  // Add it to list
  _layouts.push_back(layout.layout);
  _bindings.push_back(layout.bindings);
  _sets.push_back(dstSet);
  return *this;
}

// == Set layout builder ==

vkinit::DescriptorSetLayout vkinit::DescriptorSetLayoutBuilder::Build(const vk::Device &device,
                                                                      DeletionQueue &deletionQueue) {
  // Create layout
  vk::DescriptorSetLayoutCreateInfo setLayoutCreateInfo{
      .bindingCount = static_cast<uint32_t>(_bindings.size()),
      .pBindings = _bindings.data(),
  };

  vk::DescriptorSetLayout layout = device.createDescriptorSetLayout(setLayoutCreateInfo);

  // Register deletion
  deletionQueue.PushFunction([device, layout]() { device.destroyDescriptorSetLayout(layout); });

  // Return it
  return vkinit::DescriptorSetLayout{
      .bindings = _bindings,
      .layout = layout,
  };
}
vkinit::DescriptorSetLayoutBuilder
vkinit::DescriptorSetLayoutBuilder::AddBinding(vk::ShaderStageFlags stages,
                                               vk::DescriptorType descriptorType) {

  // Set binding
  _bindings.push_back(vk::DescriptorSetLayoutBinding{
      .binding = static_cast<uint32_t>(_bindings.size()),
      .descriptorType = descriptorType,
      .descriptorCount = 1,
      .stageFlags = stages,
      .pImmutableSamplers = nullptr,
  });

  return *this;
}

// === Set writer ===

vkinit::DescriptorSetWriter::DescriptorSetWriter(
    const std::vector<vk::DescriptorSet *> &sets,
    const std::vector<std::vector<vk::DescriptorSetLayoutBinding>> &bindings) {
  _sets = sets;
  _bindings = bindings;
}

vkinit::DescriptorSetWriter vkinit::DescriptorSetWriter::AddBuffer(uint32_t setIndex, uint32_t bindingIndex,
                                                                   vk::Buffer buffer, size_t range,
                                                                   size_t offset) {

  // Get binding
  auto binding = _bindings[setIndex][bindingIndex];

  // Set buffer info
  _bufferInfos.push_back(vk::DescriptorBufferInfo{
      .buffer = buffer,
      .offset = offset,
      .range = range,
  });

  // Set write
  _writes.push_back(vk::WriteDescriptorSet{
      .dstSet = *_sets[setIndex],
      .dstBinding = bindingIndex,
      .descriptorCount = 1,
      .descriptorType = binding.descriptorType,
      .pBufferInfo = &_bufferInfos[_bufferInfos.size() - 1],
  });

  return *this;
}

void vkinit::DescriptorSetWriter::Write(const vk::Device &device) {
  device.updateDescriptorSets(_writes, nullptr);
}
