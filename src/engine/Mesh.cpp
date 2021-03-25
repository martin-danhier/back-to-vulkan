//
// Created by Martin Danhier on 24/03/2021.
//

#include "Mesh.h"
Mesh::Mesh(std::vector<Vertex> &vertices) { _vertices = vertices; }

void Mesh::Upload(VmaAllocator allocator, DeletionQueue& deletionQueue) {

}
