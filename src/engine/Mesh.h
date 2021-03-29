//
// Created by Martin Danhier on 24/03/2021.
//

#pragma once

#include "vk_types.h"
#include <vector>
#include <glm/vec3.hpp>

struct VertexInputDescription {
  std::vector<vk::VertexInputBindingDescription> bindings{};
  std::vector<vk::VertexInputAttributeDescription> attributes{};
  vk::PipelineVertexInputStateCreateFlags flags{};
};


struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 color;

  static VertexInputDescription GetVertexDescription();
};

class Mesh {
private:
  std::vector<Vertex> _vertices;
  AllocatedBuffer _vertexBuffer;
public:
  Mesh() = default;
  explicit Mesh(std::vector<Vertex>& vertices);
  [[nodiscard]] vk::Buffer GetVertexBuffer() const;
  [[nodiscard]] size_t GetVertexCount() const;
  void Upload(VmaAllocator allocator, class DeletionQueue& deletionQueue);
};
