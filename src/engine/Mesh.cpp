//
// Created by Martin Danhier on 24/03/2021.
//

#include "Mesh.h"
#include "vk_engine.h"
#include <iostream>

Mesh::Mesh(std::vector<Vertex> &vertices) { _vertices = vertices; }

void Mesh::Upload(VmaAllocator allocator, DeletionQueue &deletionQueue) {
  // Create the allocation
  vk::BufferCreateInfo bufferCreateInfo{
      .size = _vertices.size() * sizeof(Vertex),
      .usage = vk::BufferUsageFlagBits::eVertexBuffer,
  };

  VmaAllocationCreateInfo allocationCreateInfo{
      .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
  };

  auto result = vmaCreateBuffer(
      allocator, (VkBufferCreateInfo *) &bufferCreateInfo, &allocationCreateInfo,
      (VkBuffer *)&_vertexBuffer.buffer, &_vertexBuffer.allocation, nullptr);
  if (result != 0)
    throw std::runtime_error("Unable to create allocation for vertex buffer");

  // Register the deletion
  deletionQueue.PushFunction([=]() {
    vmaDestroyBuffer(allocator, _vertexBuffer.buffer, _vertexBuffer.allocation);
  });

  // Copy vertex data
  void *data;
  vmaMapMemory(allocator, _vertexBuffer.allocation, &data);
  memcpy(data, _vertices.data(), _vertices.size() * sizeof(Vertex));
  vmaUnmapMemory(allocator, _vertexBuffer.allocation);
}
vk::Buffer Mesh::GetVertexBuffer() const { return _vertexBuffer.buffer; }

size_t Mesh::GetVertexCount() const { return _vertices.size(); }

VertexInputDescription Vertex::GetVertexDescription() {

  VertexInputDescription description;

  // Only 1 binding, on a per vertex rate
  vk::VertexInputBindingDescription mainBinding{
      .binding = 0,
      .stride = sizeof(Vertex),
      .inputRate = vk::VertexInputRate::eVertex,
  };
  description.bindings.push_back(mainBinding);

  // Vertex position attribute: location 0
  vk::VertexInputAttributeDescription positionAttribute{
      .location = 0,
      .binding = 0,
      .format = vk::Format::eR32G32B32Sfloat,
      .offset = static_cast<uint32_t>(offsetof(Vertex, position)),
  };
  description.attributes.push_back(positionAttribute);
  // Vertex normal attribute: location 1
  vk::VertexInputAttributeDescription normalAttribute{
      .location = 1,
      .binding = 0,
      .format = vk::Format::eR32G32B32Sfloat,
      .offset = static_cast<uint32_t>(offsetof(Vertex, normal)),
  };
  description.attributes.push_back(normalAttribute);
  // Vertex color attribute: location 2
  vk::VertexInputAttributeDescription colorAttribute{
      .location = 2,
      .binding = 0,
      .format = vk::Format::eR32G32B32Sfloat,
      .offset = static_cast<uint32_t>(offsetof(Vertex, color)),
  };
  description.attributes.push_back(colorAttribute);

  // Create the final struct
  return description;
}
