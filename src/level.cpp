#include <set>
#include <iostream>

#include <camera/core.hpp>

#include "level.hpp"
#include "entities.hpp"
#include "utilities.hpp"
#include "serialize.hpp"

constexpr uint16_t LEVEL_FILE_VERSION = 1U;
constexpr uint16_t LEVEL_FILE_MAGIC   = 7123U;
void saveLevel(EntityManager & entity_manager, const std::string & level_path, const Camera &camera){
    std::cout << "----------- Writing Level " << level_path << "----------\n";

    std::set<Mesh*> used_meshes;
    std::set<AnimatedMesh*> used_animated_meshes;
    std::set<Texture*> used_textures;
    for(int i = 0; i < ENTITY_COUNT; i++){
        auto e = entity_manager.entities[i];
        if(e == nullptr) continue;

        if(entityInherits(e->type, MESH_ENTITY)) {
            auto me = (MeshEntity*)e;
            if (me->mesh != nullptr)
                used_meshes.emplace(me->mesh);
            //if(me->lightmap != nullptr)
            //    used_textures.emplace(me->lightmap);
        }
        if(entityInherits(e->type, ANIMATED_MESH_ENTITY)) {
            auto ae = (AnimatedMeshEntity*)e;
            if(ae->animesh != nullptr) 
                used_animated_meshes.emplace(ae->animesh);
        }
        if (entityInherits(e->type, VEGETATION_ENTITY)) {
            auto ve = (VegetationEntity*)e;
            if(ve->texture != nullptr)
                used_textures.emplace(ve->texture);
        }
    }

    FILE *f;
    f=fopen(level_path.c_str(), "wb");

    fwrite(&LEVEL_FILE_MAGIC  , sizeof(LEVEL_FILE_MAGIC  ), 1, f);
    fwrite(&LEVEL_FILE_VERSION, sizeof(LEVEL_FILE_VERSION), 1, f);

    writeCamera(camera, f);

    //
    // Write assets by writing type followed by list of paths
    // @todo maybe fixed sized handles
    //
    std::unordered_map<uint64_t, uint64_t> asset_lookup; // maps asset pointer to index into path list
    uint64_t asset_index = 0;

    auto asset_type = MESH_ASSET;
    uint16_t num_mesh_asset = used_meshes.size();

    if(num_mesh_asset) {
        fwrite(&asset_type, sizeof(asset_type), 1, f);
        fwrite(&num_mesh_asset, sizeof(num_mesh_asset), 1, f);

        for(const auto &mesh : used_meshes){
            asset_lookup[(uint64_t)mesh] = asset_index;

            uint8_t len = mesh->handle.size();
            fwrite(&len, sizeof(len), 1, f);
            fwrite(mesh->handle.data(), len, 1, f);

            ++asset_index;
        }
    }

    asset_type = ANIMATED_MESH_ASSET;
    uint16_t num_animated_mesh_asset = used_animated_meshes.size();

    if(num_animated_mesh_asset) {
        fwrite(&asset_type, sizeof(asset_type), 1, f);
        fwrite(&num_animated_mesh_asset, sizeof(num_animated_mesh_asset), 1, f);

        for(const auto &animesh : used_animated_meshes){
            asset_lookup[(uint64_t)animesh] = asset_index;

            uint8_t len = animesh->handle.size();
            fwrite(&len, sizeof(len), 1, f);
            fwrite(animesh->handle.data(), len, 1, f);

            ++asset_index;
        }
    }

    asset_type = TEXTURE_ASSET;
    uint16_t num_texture_asset = used_textures.size();

    if(num_texture_asset) {
        fwrite(&asset_type, sizeof(asset_type), 1, f);
        fwrite(&num_texture_asset, sizeof(num_texture_asset), 1, f);

        for(const auto &texture : used_textures){
            asset_lookup[(uint64_t)texture] = asset_index;

            uint8_t len = texture->handle.size();
            fwrite(&len, sizeof(len), 1, f);
            fwrite(texture->handle.data(), len, 1, f);

            uint16_t format = texture->format;
            fwrite(&format, sizeof(format), 1, f);

            ++asset_index;
        }
    }

    // Write NONE_ASSET to signal end of asset path writing 
    asset_type = NONE_ASSET;
    fwrite(&asset_type, sizeof(asset_type), 1, f);

    //
    // Actually write list of entities, replacing any asset pointers with indices
    //
    for(int i = 0; i < ENTITY_COUNT; i++){
        auto e = entity_manager.entities[i];
        if(e == nullptr || e->type == ENTITY) continue;

        fwrite(&e->type, sizeof(EntityType), 1, f);

        writeEntity(e, f);
        if(entityInherits(e->type, MESH_ENTITY)) {
            writeMeshEntity((MeshEntity*)e, asset_lookup, f);
        }
        if(entityInherits(e->type, ANIMATED_MESH_ENTITY)) {
            writeAnimatedMeshEntity((AnimatedMeshEntity*)e, asset_lookup, f);
        }
        if(entityInherits(e->type, WATER_ENTITY)) {
            writeWaterEntity((WaterEntity*)e, f);
        }
        if(entityInherits(e->type, COLLIDER_ENTITY)) {
            writeColliderEntity((ColliderEntity*)e, f);
        }
        if(entityInherits(e->type, VEGETATION_ENTITY)) {
            writeVegetationEntity((VegetationEntity*)e, asset_lookup, f);
        }
        if (entityInherits(e->type, PLAYER_ENTITY)) {
            writePlayerEntity((PlayerEntity*)e, f);
        }
    }

    fclose(f);
}

// Overwrites existing entity indices
// @todo if needed make loading asign new ids such that connections are maintained
// @todo any entities that store ids must be resolved so that invalid ie wrong versions are NULLID 
// since we dont maintain version either
bool loadLevel(EntityManager &entity_manager, AssetManager &asset_manager, const std::string &level_path, Camera &camera) {
    std::cout << "---------- Loading Level " << level_path << "----------\n";

    FILE *f;
    f=fopen(level_path.c_str(),"rb");

    if (!f) {
        // @debug
        std::cerr << "Error in reading level file at path: " << level_path << ".\n";
        return false;
    }

    std::remove_cv_t<decltype(LEVEL_FILE_MAGIC)> magic;
    fread(&magic, sizeof(magic), 1, f);
    if (magic != LEVEL_FILE_MAGIC) {
        std::cerr << "Invalid level file magic, was " << magic << " expected " << LEVEL_FILE_MAGIC << ".\n";
        return false;
    }

    std::remove_cv_t<decltype(LEVEL_FILE_VERSION)> version;
    fread(&version, sizeof(version), 1, f);
    if(version != LEVEL_FILE_VERSION){
        std::cerr << "Invalid level file version, was " << version << " expected " << LEVEL_FILE_VERSION << ".\n";
        return false;
    }

    readCamera(camera, f);

    uint64_t asset_index = 0;
    std::unordered_map<uint64_t, void *> index_to_asset;
    std::set<std::string> asset_handles;

    AssetType asset_type;
    while(1) {
        fread(&asset_type, sizeof(asset_type), 1, f);
        if(asset_type == NONE_ASSET) break;

        uint16_t num_assets;
        fread(&num_assets, sizeof(num_assets), 1, f);

        std::cout << "Reading " << num_assets << " asset/s of type " << (int)asset_type << ".\n";

        for(uint16_t i = 0; i < num_assets; ++i) {
            uint8_t handle_len;
            fread(&handle_len, sizeof(handle_len), 1, f);

            std::string handle;
            handle.resize(handle_len);
            fread(handle.data(), handle_len, 1, f);
            asset_handles.emplace(handle);

            // @perf move out of loop
            switch (asset_type) {
                case MESH_ASSET:
                {
                    auto mesh = asset_manager.getMesh(handle);
                    if (mesh == nullptr) {
                        // In future this should probably be true for all meshes by saving to mesh
                        mesh = asset_manager.createMesh(handle);
                        if (endsWith(handle, ".mesh")) {
                            asset_manager.loadMeshFile(mesh, handle);
                        }
                        else {
                            std::cerr << "Warning, there is a model file being loaded with assimp\n";
                            asset_manager.loadMeshAssimp(mesh, handle);
                        }
                    }

                    index_to_asset[asset_index] = (void*)mesh;
                    break;
                }
                case ANIMATED_MESH_ASSET:
                {
                    auto animesh = asset_manager.getAnimatedMesh(handle);
                    if (animesh == nullptr) {
                        animesh = asset_manager.createAnimatedMesh(handle);
                        asset_manager.loadAnimationFile(animesh, handle);
                    }

                    index_to_asset[asset_index] = (void*)animesh;
                    break;
                }
                case TEXTURE_ASSET:
                {
                    uint16_t format;
                    fread(&format, sizeof(format), 1, f);

                    auto texture = asset_manager.getTexture(handle);
                    if (texture == nullptr) {
                        // @todo internal_format
                        texture = asset_manager.createTexture(handle);
                        asset_manager.loadTexture(texture, handle, format);
                    }

                    index_to_asset[asset_index] = (void*)texture;
                    break;
                }
                default:
                {
                    std::cerr << "Unhandled asset type " << asset_type << " encountered reading level\n";
                    break;
                }
            }

            asset_index++;
        }
    }
    //asset_manager.clearExcluding(asset_handles);

    // 
    // Read entities until end of file
    //
    entity_manager.clear();
    has_played = false;
    char c;
    while((c = fgetc(f)) != EOF){
        ungetc(c, f);

        // If needed we can write id as well to maintain during saves
        EntityType type;
        fread(&type, sizeof(EntityType), 1, f);

        auto e = allocateEntity(NULLID, type);
        readEntity(e, f);

        if(entityInherits(e->type, MESH_ENTITY)) {
            readMeshEntity((MeshEntity*)e, index_to_asset, f);
        }
        if(entityInherits(e->type, ANIMATED_MESH_ENTITY)) {
            readAnimatedMeshEntity((AnimatedMeshEntity*)e, index_to_asset, f);
        }
        if(entityInherits(e->type, WATER_ENTITY)) {
            readWaterEntity((WaterEntity*)e, f);
        }
        if(entityInherits(e->type, COLLIDER_ENTITY)) {
            readColliderEntity((ColliderEntity*)e, f);
        }
        if(entityInherits(e->type, VEGETATION_ENTITY)) {
            readVegetationEntity((VegetationEntity*)e, index_to_asset, f);
        }
        if (entityInherits(e->type, PLAYER_ENTITY)) {
            readPlayerEntity((PlayerEntity*)e, f);
        }

        // Water and players are special cases since there can only be one per level
        bool unique = entityInherits(e->type, WATER_ENTITY) || entityInherits(e->type, PLAYER_ENTITY);
        if(unique) {
            if (entityInherits(e->type, PLAYER_ENTITY)) {
                if (entity_manager.player != NULLID) {
                    std::cerr << "Duplicate player in level, skipping\n";
                    free(e);
                    continue;
                }
                else {
                    entity_manager.setEntity(entity_manager.getFreeId().i, e);
                    entity_manager.player = e->id;
                }
            }  else if (entityInherits(e->type, WATER_ENTITY)) {
                if(entity_manager.water != NULLID) {
                    std::cerr << "Duplicate water in level, skipping\n";
                    free(e);
                    continue;
                } else {
                    entity_manager.setEntity(entity_manager.getFreeId().i, e);
                    entity_manager.water = e->id;
                }
            }
        } else {
            // @todo preserve ids, you probably need to write free entity ids or
            // reorder to preserve relationships but remove gaps
            entity_manager.setEntity(entity_manager.getFreeId().i, e);
        }

        std::cout << "Loaded entity of type " << type << " with id " << e->id.i << ".\n";
    }
    fclose(f);

    // Update water collision map
    if (entity_manager.water != NULLID) {
        // @todo make better systems for determining when to update shadow map
        auto water = (WaterEntity*)entity_manager.getEntity(entity_manager.water);
        if (water != nullptr) {
            bindDrawWaterColliderMap(entity_manager, water);
            distanceTransformWaterFbo(water);
        }
        else {
            entity_manager.water = NULLID;
        }
    }

    return true;
}

