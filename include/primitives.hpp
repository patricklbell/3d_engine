#ifndef ENGINE_PRIMITIVES_HPP
#define ENGINE_PRIMITIVES_HPP

#include "assets.hpp"

void createTessellatedGridMesh(Mesh* g, glm::uvec2 tiles);
void createQuadMesh(Mesh* q);
void createCubeMesh(Mesh* c);
void createLineCubeMesh(Mesh* c);

#endif // ENGINE_PRIMITIVES_HPP