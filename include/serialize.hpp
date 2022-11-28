#ifndef SERIALIZE_HPP
#define SERIALIZE_HPP

#include "graphics.hpp"
#include "entities.hpp"

// @note These functions only read the part which concerns their specific type, ie. call readMesh then readAnimatedMesh
void writeEntity(Entity* e, FILE* f);
void readEntity(Entity * e, FILE * f);
void writeMeshEntity(MeshEntity * e, std::unordered_map<uint64_t, uint64_t> asset_lookup, FILE * f);
void readMeshEntity(MeshEntity * e, const std::unordered_map<uint64_t, void*> index_to_asset, FILE * f);
void writeAnimatedMeshEntity(AnimatedMeshEntity * e, std::unordered_map<uint64_t, uint64_t> asset_lookup, FILE * f);
void readAnimatedMeshEntity(AnimatedMeshEntity * e, const std::unordered_map<uint64_t, void*> index_to_asset, FILE * f);
void writeWaterEntity(WaterEntity * e, FILE * f);
void readWaterEntity(WaterEntity * e, FILE * f);
void writeColliderEntity(ColliderEntity * e, FILE * f);
void readColliderEntity(ColliderEntity * e, FILE * f);
void writePlayerEntity(PlayerEntity * e, FILE * f);
void readPlayerEntity(PlayerEntity * e, FILE * f);
void writeVegetationEntity(VegetationEntity * e, std::unordered_map<uint64_t, uint64_t> asset_lookup, FILE * f);
void readVegetationEntity(VegetationEntity * e, const std::unordered_map<uint64_t, void*> index_to_asset, FILE * f);

void writeCamera(const Camera & camera, FILE * f);
void readCamera(Camera & camera, FILE * f);

void writeMaterial(const Material& mat, FILE* f);
void readMaterial(Material& mat, AssetManager& asset_manager, FILE* f);

void writeMesh(const Mesh& mesh, FILE* f);
bool readMesh(Mesh& mesh, AssetManager& assets, FILE* f);

#endif // SERIALIZE_HPP
