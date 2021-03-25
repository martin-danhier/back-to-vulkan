//
// Created by Martin Danhier on 24/03/2021.
//

#pragma once

#include "vk_engine.h"
#include <vector>
#include <glm/vec3.hpp>

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 color;
};

class Mesh {
private:
  std::vector<Vertex> _vertices;
  AllocatedBuffer _vertexBuffer;
public:
  explicit Mesh(std::vector<Vertex>& vertices);
  void Upload(VmaAllocator allocator, DeletionQueue& deletionQueue);
};
