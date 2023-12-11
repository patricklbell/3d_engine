#ifndef ENGINE_SERIALIZE_HPP
#define ENGINE_SERIALIZE_HPP

#include "graphics.hpp"
#include "entities.hpp"

void writeString(std::string_view str, FILE* f);
void readString(std::string& str, FILE* f);

// @note These functions only read the part which concerns their specific type, ie. call readMesh then readAnimatedMesh
void writeEntity(Entity* e, FILE* f);
void readEntity(Entity* e, FILE* f);
void writeMeshEntity(MeshEntity* e, FILE* f);
typedef std::tuple<Texture*, ImageData*> texture_imagedata_t;
void readMeshEntity(MeshEntity* e, AssetManager& assets, std::vector<texture_imagedata_t>& texture_imagedata_list, FILE* f);
void writeAnimatedMeshEntity(AnimatedMeshEntity* e, FILE* f);
void readAnimatedMeshEntity(AnimatedMeshEntity* e, AssetManager& assets, FILE* f);
void writeWaterEntity(WaterEntity* e, FILE* f);
void readWaterEntity(WaterEntity* e, FILE* f);
void writePointLightEntity(PointLightEntity* e, FILE* f);
void readPointLightEntity(PointLightEntity* e, FILE* f);
void writeColliderEntity(ColliderEntity* e, FILE* f);
void readColliderEntity(ColliderEntity* e, FILE* f);
void writePlayerEntity(PlayerEntity* e, FILE* f);
void readPlayerEntity(PlayerEntity* e, FILE* f);

void writeCamera(const Camera& camera, FILE* f);
void readCamera(Camera& camera, FILE* f);

void writeEnvironment(const Environment& env, FILE* f);
void readEnvironment(Environment& env, AssetManager& assets, FILE* f);

void writeMaterial(const Material& mat, FILE* f);
void readMaterial(Material& mat, AssetManager& assets, FILE* f);

void writeMesh(const Mesh& mesh, FILE* f);
bool readMesh(Mesh& mesh, AssetManager& assets, FILE* f);

void writeLevel(Level& level, FILE* f);
bool readLevel(Level& level, AssetManager& asset_manager, FILE* f);

#endif // ENGINE_SERIALIZE_HPP
