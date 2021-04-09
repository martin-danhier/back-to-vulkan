//
// Created by Martin Danhier on 15/03/2021.
//

#include "vk_engine.h"
#include "vk_init.h"

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


// ====

vkinit::DescriptorSetBuilder::DescriptorSetBuilder(vk::DescriptorPool &pool, vk::Device &device, DeletionQueue &deletionQueue) {
  _pool = pool;
  _device = device;
  _deletionQueue = deletionQueue;
}
vkinit::DescriptorSetBuilder vkinit::DescriptorSetBuilder::SaveDescriptorSet(vk::DescriptorSetLayout *layout, vk::DescriptorSet *set) {
  // Create layout
  vk::DescriptorSetLayoutCreateInfo setLayoutCreateInfo{
      .bindingCount = static_cast<uint32_t>(_bindings.size()),
      .pBindings = _bindings.data(),
  };
  *layout = _device.createDescriptorSetLayout(setLayoutCreateInfo);
  _layouts.push_back(*layout);
  _bindingCounts.push_back(_bindings.size());

  // Register deletion
  _deletionQueue.PushFunction([this, layout](){
    _device.destroyDescriptorSetLayout(*layout);
  });

  // Save set pointer
  _sets.push_back(set);

  // Reset bindings
  _bindings.clear();

  return *this;

}
vkinit::DescriptorSetBuilder vkinit::DescriptorSetBuilder::AddBuffer(vk::ShaderStageFlags stages, vk::DescriptorType descriptorType, const vk::Buffer &buffer, size_t range, size_t offset) {
  uint32_t bindingIndex = _bindings.size();
  // Set binding
  _bindings.push_back(vk::DescriptorSetLayoutBinding{
      .binding =  bindingIndex,
      .descriptorType = descriptorType,
      .descriptorCount = 1,
      .stageFlags = stages,
      .pImmutableSamplers = nullptr,
  });
  // Set buffer info
  vk::DescriptorBufferInfo bufferInfo {
    .buffer = buffer,
    .offset = offset,
    .range = range,
  };

  // Set write
  _writes.push_back(vk::WriteDescriptorSet{
      .dstBinding = bindingIndex,
      .descriptorCount = 1,
      .descriptorType = descriptorType,
      .pBufferInfo = &bufferInfo,
  });

  return *this;
}
void vkinit::DescriptorSetBuilder::Build() {
  // Allocate sets
  vk::DescriptorSetAllocateInfo setAllocInfo{
        .descriptorPool = _pool,
        .descriptorSetCount = static_cast<uint32_t>(_layouts.size()),
        .pSetLayouts = _layouts.data(),
    };
  auto sets = _device.allocateDescriptorSets(setAllocInfo);
  // Save sets
  uint32_t w = 0;
  for (uint32_t i = 0; i < sets.size(); i++) {
    *_sets[i] = sets[i];
    // Update writes
    for (uint32_t j = w; j < w + _bindingCounts[i]; j++) {
      _writes[j].dstSet = sets[i];
    }
    w += _bindingCounts[i];
  }

  // Update
  _device.updateDescriptorSets(_writes, nullptr);
}