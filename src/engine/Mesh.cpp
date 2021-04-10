//
// Created by Martin Danhier on 24/03/2021.
//

#include "Mesh.h"
#include "vk_engine.h"
#include <iostream>
#include <tiny_obj_loader.h>

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
      allocator, (VkBufferCreateInfo *)&bufferCreateInfo, &allocationCreateInfo,
      (VkBuffer *)&_vertexBuffer.buffer, &_vertexBuffer.allocation, nullptr);
  if (result != 0)
    throw std::runtime_error("Unable to create allocation for vertex buffer");

  // Register the deletion
  deletionQueue.PushFunction([this, allocator]() {
    vmaDestroyBuffer(allocator, _vertexBuffer.buffer, _vertexBuffer.allocation);
  });

  // Copy vertex data
  void *data = nullptr;
  vmaMapMemory(allocator, _vertexBuffer.allocation, &data);
  memcpy(data, _vertices.data(), _vertices.size() * sizeof(Vertex));
  vmaUnmapMemory(allocator, _vertexBuffer.allocation);
}
vk::Buffer &Mesh::GetVertexBuffer() { return _vertexBuffer.buffer; }

size_t Mesh::GetVertexCount() const { return _vertices.size(); }

bool Mesh::LoadFromObj(const char *filename) {
  // Attrib will contain the vertex arrays
  tinyobj::attrib_t attrib;
  // Shapes contains the infos for each separate object in the file
  std::vector<tinyobj::shape_t> shapes;
  // Material contains the infos about the material of each shape
  std::vector<tinyobj::material_t> materials;

  // Store errors and warnings
  std::string warn;
  std::string err;

  // Load the OBJ file
  tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename,
                   nullptr);

  // Print warnings and errors
  if (!warn.empty()) {
    std::cout << "OBJ WARN: " << warn << std::endl;
  }
  // If we have any error, print it to the console, and break the mesh loading.
  // This happens if the file can't be found or is malformed
  if (!err.empty()) {
    std::cerr << err << std::endl;
    return false;
  }

  // Load the data

  // Hardcode the loading of triangles
  constexpr int32_t VERTEX_PER_FACE = 3;

  // For each shape (separate object in the file)
  for (auto & shape : shapes) {
    size_t indexOffset = 0;

    // For each face
    for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {

      // For each vertex of that face
      for (size_t v = 0; v < VERTEX_PER_FACE; v++) {
        tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];

        // vertex position
        tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
        tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
        tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
        // vertex normal
        tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
        tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
        tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];

        // Position
        Vertex newVertex{
            .position{vx, vy, vz},
            .normal{nx, ny, nz},
            .color{nx, ny, nz},
        };

        _vertices.push_back(newVertex);
      }

      indexOffset += VERTEX_PER_FACE;
    }
  }

  return true;
}
std::vector<Vertex> Mesh::GetVertices() const { return _vertices; }
VmaAllocation &Mesh::GetAllocation() { return _vertexBuffer.allocation; }

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
