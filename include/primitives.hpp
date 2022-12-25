#ifndef PRIMITIVES_HPP
#define PRIMITIVES_HPP

#include "assets.hpp"

void createTessellatedGridMesh(Mesh* g, glm::ivec2 tiles);
void createQuadMesh(Mesh* q);
void createCubeMesh(Mesh* c);
void createLineCubeMesh(Mesh* c);

#endif // PRIMITIVES_HPP