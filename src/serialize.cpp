#include <stdlib.h>
#include <functional>

#include <utilities/math.hpp>
#include "serialize.hpp"
#include "entities.hpp"
#include "globals.hpp"
#include "assets.hpp"
#include "level.hpp"
#include "threadpool.hpp"

static void loadTextureImageDataList(std::vector<texture_imagedata_t>& texture_imagedata_list) {
    for (auto& tpl : texture_imagedata_list) {
        auto& tex = std::get<0>(tpl);
        auto& img = std::get<1>(tpl);

#if DO_MULTITHREAD 
        global_thread_pool->queueJob(std::bind(loadImageData, img, tex->handle, getChannelsForFormat(tex->format), false, true));
#else
        loadImageData(img, tex->handle, getChannelsForFormat(tex->format), false, true);
#endif
    }

#if DO_MULTITHREAD 
    // Block main thread until texture loading is finished
    while (global_thread_pool->busy()) {}
#endif

    // Now transfer loaded data into actual textures
    for (auto& tpl : texture_imagedata_list) {
        auto& tex = std::get<0>(tpl);
        auto& img = std::get<1>(tpl);

        if (tex->format == GL_FALSE) {
            tex->format = getFormatForChannels(img->available_n);
        }
        tex->id = createGLTextureFromData(img, tex->format, GL_REPEAT);
        tex->resolution = glm::vec2(img->x, img->y);
        tex->complete = tex->id != GL_FALSE;
        delete img;
    }
}
    
void writeString(std::string_view str, FILE* f) {
    if(str.size() > (uint8_t)-1) {
        std::cerr << "Handle " << str<< " is too long\n";
    }
    uint8_t len = str.size();
    fwrite(&len, sizeof(len), 1, f);
    fwrite(str.data(), len, 1, f);
}

void readString(std::string &str, FILE* f) {
    uint8_t len;
    fread(&len, sizeof(len), 1, f);

    str.resize(len);
    fread(&str[0], sizeof(char) * len, 1, f);
}

void writeEntity(Entity* e, FILE* f) {
    fwrite(&e->id.i, sizeof(e->id.i), 1, f);
    fwrite(&e->id.v, sizeof(e->id.v), 1, f);
}
void readEntity(Entity* e, FILE* f) {
    fread(&e->id.i, sizeof(e->id.i), 1, f);
    fread(&e->id.v, sizeof(e->id.v), 1, f);
}

void writeMeshEntity(MeshEntity* e, FILE* f) {
    fwrite(&e->position, sizeof(e->position), 1, f);
    fwrite(&e->rotation, sizeof(e->rotation), 1, f);
    fwrite(&e->scale, sizeof(e->scale), 1, f);

    // @editor, it will be annoying but the level file will probably have to be translated for release
    fwrite(&e->gizmo_position_offset, sizeof(e->gizmo_position_offset), 1, f);

    writeString(e->mesh ? e->mesh->handle : "", f);

    fwrite(&e->casts_shadow, sizeof(e->casts_shadow), 1, f);
    fwrite(&e->do_lightmap, sizeof(e->do_lightmap), 1, f);
    
    uint64_t num_overriden_materials = e->overidden_materials.size();
    fwrite(&num_overriden_materials, sizeof(num_overriden_materials), 1, f);
    for (const auto& p : e->overidden_materials) {
        fwrite(&p.first, sizeof(p.first), 1, f);
        writeMaterial(p.second, f);
    }
}
void readMeshEntity(MeshEntity* e, AssetManager& assets, std::vector<texture_imagedata_t>& texture_imagedata_list, FILE* f) {
    fread(&e->position, sizeof(e->position), 1, f);
    fread(&e->rotation, sizeof(e->rotation), 1, f);
    fread(&e->scale, sizeof(e->scale), 1, f);

    fread(&e->gizmo_position_offset, sizeof(e->gizmo_position_offset), 1, f);

    std::string handle;
    readString(handle, f);
    if (handle != "") {
        e->mesh = assets.getMesh(handle);
        if (e->mesh == nullptr) {
            e->mesh = assets.createMesh(handle);
            assets.loadMeshFile(e->mesh, handle);
        }
    }

    fread(&e->casts_shadow, sizeof(e->casts_shadow), 1, f);
    fread(&e->do_lightmap, sizeof(e->do_lightmap), 1, f);

    uint64_t num_overriden_materials;
    fread(&num_overriden_materials, sizeof(num_overriden_materials), 1, f);
    e->overidden_materials.reserve(num_overriden_materials);
    for (uint64_t i = 0; i < num_overriden_materials; i++) {
        uint64_t mat_i;
        fread(&mat_i, sizeof(mat_i), 1, f);
        readMaterial(e->overidden_materials[mat_i], assets, f);

        for (auto& p : e->overidden_materials[mat_i].textures) {
            if (p.second && !p.second->complete) {
                auto img_data = new ImageData();
                texture_imagedata_list.emplace_back(texture_imagedata_t{ p.second, img_data });
            }
        }
    }
}

void writeAnimatedMeshEntity(AnimatedMeshEntity* e, FILE* f) {
    writeString(e->animesh ? e->animesh->handle : "", f);

    fwrite(&e->default_event.current_time, sizeof(e->default_event.current_time), 1, f);
    fwrite(&e->default_event.time_scale, sizeof(e->default_event.time_scale), 1, f);
    fwrite(&e->default_event.loop, sizeof(e->default_event.loop), 1, f);
    fwrite(&e->default_event.playing, sizeof(e->default_event.playing), 1, f);

    writeString(e->default_event.animation ? e->default_event.animation->name : "", f);
}
void readAnimatedMeshEntity(AnimatedMeshEntity* e, AssetManager& assets, FILE* f) {
    std::string handle;
    readString(handle, f);
    if (handle != "") {
        e->animesh = assets.getAnimatedMesh(handle);
        if (e->animesh == nullptr) {
            e->animesh = assets.createAnimatedMesh(handle);
            assets.loadAnimationFile(e->animesh, handle);
        }
    }

    fread(&e->default_event.current_time, sizeof(e->default_event.current_time), 1, f);
    fread(&e->default_event.time_scale, sizeof(e->default_event.time_scale), 1, f);
    fread(&e->default_event.loop, sizeof(e->default_event.loop), 1, f);
    fread(&e->default_event.playing, sizeof(e->default_event.playing), 1, f);

    std::string animation_name;
    readString(animation_name, f);
    if (e->animesh) {
        auto lu = e->animesh->name_animation_map.find(animation_name);
        if (lu != e->animesh->name_animation_map.end()) {
            e->default_event.animation = &lu->second;
        } else {
            std::cerr << "Unknown animation " << animation_name << " when reading animated mesh entity\n";
        }
    }
    else {
        std::cerr << "Animation name " << animation_name << " was set but animesh was null, something went wrong\n";
    }
}

void writeWaterEntity(WaterEntity* e, FILE* f) {
    fwrite(&e->position, sizeof(e->position), 1, f);
    fwrite(&e->scale, sizeof(e->scale), 1, f);

    fwrite(&e->shallow_color, sizeof(e->shallow_color), 1, f);
    fwrite(&e->deep_color, sizeof(e->deep_color), 1, f);
    fwrite(&e->foam_color, sizeof(e->foam_color), 1, f);
}
void readWaterEntity(WaterEntity* e, FILE* f) {
    fread(&e->position, sizeof(e->position), 1, f);
    fread(&e->scale, sizeof(e->scale), 1, f);

    fread(&e->shallow_color, sizeof(e->shallow_color), 1, f);
    fread(&e->deep_color, sizeof(e->deep_color), 1, f);
    fread(&e->foam_color, sizeof(e->foam_color), 1, f);
}

void writeColliderEntity(ColliderEntity* e, FILE* f) {
    fwrite(&e->collider_position, sizeof(e->collider_position), 1, f);
    fwrite(&e->collider_rotation, sizeof(e->collider_rotation), 1, f);
    fwrite(&e->collider_scale, sizeof(e->collider_scale), 1, f);

    fwrite(&e->selectable, sizeof(e->selectable), 1, f);
}
void readColliderEntity(ColliderEntity* e, FILE* f) {
    fread(&e->collider_position, sizeof(e->collider_position), 1, f);
    fread(&e->collider_rotation, sizeof(e->collider_rotation), 1, f);
    fread(&e->collider_scale, sizeof(e->collider_scale), 1, f);

    fread(&e->selectable, sizeof(e->selectable), 1, f);
}

void writePlayerEntity(PlayerEntity* e, FILE* f) {
}
void readPlayerEntity(PlayerEntity* e, FILE* f) {
}

void writeCamera(const Camera& camera, FILE* f) {
    fwrite(&camera.position, sizeof(camera.position), 1, f);
    fwrite(&camera.target, sizeof(camera.target), 1, f);
}
void readCamera(Camera& camera, FILE* f) {
    fread(&camera.position, sizeof(camera.position), 1, f);
    fread(&camera.target, sizeof(camera.target), 1, f);
    camera.view_updated = true;
    camera.update();
}

void writeEnvironment(const Environment& env, FILE* f) {
    writeString(env.skybox->handle, f);

    fwrite(&env.fog.anisotropy, sizeof(env.fog.anisotropy), 1, f);
    fwrite(&env.fog.density, sizeof(env.fog.density), 1, f);
    fwrite(&env.fog.noise_amount, sizeof(env.fog.noise_amount), 1, f);
    fwrite(&env.fog.noise_scale, sizeof(env.fog.noise_scale), 1, f);
}
void readEnvironment(Environment& env, AssetManager& assets, FILE* f) {
    std::string handle;
    readString(handle, f);
    createEnvironmentFromCubemap(env, assets, handle, GL_RGB16F);
    
    fread(&env.fog.anisotropy, sizeof(env.fog.anisotropy), 1, f);
    fread(&env.fog.density, sizeof(env.fog.density), 1, f);
    fread(&env.fog.noise_amount, sizeof(env.fog.noise_amount), 1, f);
    fread(&env.fog.noise_scale, sizeof(env.fog.noise_scale), 1, f);
}

void writeMaterial(const Material& mat, FILE* f) {
    static const auto write_texture = [&f](Texture* tex, FILE* f) {
        uint64_t format = tex->format;
        fwrite(&format, sizeof(format), 1, f);

        uint8_t is_color = tex->is_color;
        fwrite(&is_color, sizeof(is_color), 1, f);

        if (is_color) {
            fwrite(&tex->color, sizeof(tex->color), 1, f);
        }
        else {
            writeString(tex->handle, f);
        }
    };

    static const auto write_uniform = [&f](const Uniform &u, FILE* f) {
        fwrite(&u.type, sizeof(u.type), 1, f);
        fwrite(u.data, Uniform::size(u.type), 1, f);
    };

    fwrite(&mat.type, sizeof(mat.type), 1, f);

    uint64_t num_textures = 0;
    for (const auto& p : mat.textures) {
        if (p.second) {
            num_textures++;
        }
    }
    fwrite(&num_textures, sizeof(num_textures), 1, f);
    for (auto& p : mat.textures) {
        Texture* tex = p.second;
        TextureSlot slot = p.first;

        if (tex) {    
            fwrite(&slot, sizeof(slot), 1, f);
            write_texture(tex, f);
        }
    }

    uint64_t num_uniforms = mat.uniforms.size();
    fwrite(&num_uniforms, sizeof(num_uniforms), 1, f);
    for (const auto& p : mat.uniforms) {
        const auto& name = p.first;
        const auto& uniform = p.second;

        writeString(name, f);
        write_uniform(uniform, f);
    }
}

void readMaterial(Material& mat, AssetManager& asset_manager, FILE* f) {
    static const auto read_texture = [&asset_manager](FILE* f) {
        Texture* tex;

        uint64_t format;
        fread(&format, sizeof(format), 1, f);

        uint8_t is_color;
        fread(&is_color, sizeof(is_color), 1, f);
        
        if (is_color) {
            glm::vec4 color;
            fread(&color, sizeof(color), 1, f);
            tex = asset_manager.getColorTexture(color, format == GL_FALSE ? GL_RGBA : format);
        } else {
            std::string path;
            readString(path, f);

            auto lu = asset_manager.getTexture(path);
            if (lu != nullptr) {
                // Check if loaded texture contains enough channels,
                // if not we need to load it again with the correct channels, @note in future with our own texture
                // files all the formats should be determined by the file
                if (getChannelsForFormat(lu->format) < getChannelsForFormat(format)) {
                    lu->format = format;
                    lu->complete = false;
                }
                tex = lu;
            } else {
                tex = asset_manager.createTexture(path);
                tex->format = format;
            }
        }
        return tex;
    };

    static const auto read_uniform = [](FILE* f) {
        Uniform::Type type;
        fread(&type, sizeof(type), 1, f);

        const auto& size = Uniform::size(type);
        void* data = malloc(size);
        fread(data, size, 1, f);

        return std::move(Uniform(data, type));
    };

    fread(&mat.type, sizeof(mat.type), 1, f);

    uint64_t num_textures;
    fread(&num_textures, sizeof(num_textures), 1, f);
    for (uint64_t i = 0; i < num_textures; i++) {
        TextureSlot slot;
        fread(&slot, sizeof(slot), 1, f);
        mat.textures[slot] = read_texture(f);
    }

    uint64_t num_uniforms;
    fread(&num_uniforms, sizeof(num_uniforms), 1, f);
    for (uint64_t i = 0; i < num_uniforms; i++) {
        std::string name;

        readString(name, f);
        mat.uniforms.emplace(name, read_uniform(f));
    }
}

constexpr uint16_t MESH_FILE_VERSION = 5U;
constexpr uint16_t MESH_FILE_MAGIC = 7543U;
// For now dont worry about size of types on different platforms
void writeMesh(const Mesh& mesh, FILE* f) {
    fwrite(&MESH_FILE_MAGIC, sizeof(MESH_FILE_MAGIC), 1, f);
    fwrite(&MESH_FILE_VERSION, sizeof(MESH_FILE_VERSION), 1, f);

    fwrite(&mesh.num_indices, sizeof(mesh.num_indices), 1, f);
    fwrite(mesh.indices, sizeof(*mesh.indices), mesh.num_indices, f);

    fwrite(&mesh.attributes, sizeof(mesh.attributes), 1, f);

    fwrite(&mesh.num_vertices, sizeof(mesh.num_vertices), 1, f);
    if (mesh.attributes & MESH_ATTRIBUTES_VERTICES)
        fwrite(mesh.vertices, sizeof(*mesh.vertices), mesh.num_vertices, f);
    if (mesh.attributes & MESH_ATTRIBUTES_NORMALS)
        fwrite(mesh.normals, sizeof(*mesh.normals), mesh.num_vertices, f);
    if (mesh.attributes & MESH_ATTRIBUTES_TANGENTS)
        fwrite(mesh.tangents, sizeof(*mesh.tangents), mesh.num_vertices, f);
    if (mesh.attributes & MESH_ATTRIBUTES_UVS)
        fwrite(mesh.uvs, sizeof(*mesh.uvs), mesh.num_vertices, f);
    if (mesh.attributes & MESH_ATTRIBUTES_BONES) {
        fwrite(mesh.bone_ids, sizeof(*mesh.bone_ids), mesh.num_vertices, f);
        fwrite(mesh.weights, sizeof(*mesh.weights), mesh.num_vertices, f);
    }
    if (mesh.attributes & MESH_ATTRIBUTES_COLORS)
        fwrite(mesh.colors, sizeof(*mesh.colors), mesh.num_vertices, f);

    fwrite(&mesh.num_materials, sizeof(mesh.num_materials), 1, f);
    for (int i = 0; i < mesh.num_materials; i++) {
        writeMaterial(mesh.materials[i], f);
    }

    fwrite(&mesh.num_submeshes, sizeof(mesh.num_submeshes), 1, f);

    // Write material indice ranges
    fwrite(mesh.material_indices, sizeof(*mesh.material_indices), mesh.num_submeshes, f);
    fwrite(mesh.draw_start, sizeof(*mesh.draw_start), mesh.num_submeshes, f);
    fwrite(mesh.draw_count, sizeof(*mesh.draw_count), mesh.num_submeshes, f);
    fwrite(mesh.transforms, sizeof(*mesh.transforms), mesh.num_submeshes, f);
    for (int i = 0; i < mesh.num_submeshes; i++) { writeString(mesh.submesh_names[i], f); }
}

bool readMesh(Mesh& mesh, AssetManager& assets, FILE* f) {
    std::remove_cv_t<decltype(MESH_FILE_MAGIC)> magic;
    fread(&magic, sizeof(magic), 1, f);
    if (magic != MESH_FILE_MAGIC) {
        std::cerr << "Invalid mesh file magic, was " << magic << " expected " << MESH_FILE_MAGIC << ".\n";
        return false;
    }

    std::remove_cv_t<decltype(MESH_FILE_VERSION)> version;
    fread(&version, sizeof(version), 1, f);
    if (version != MESH_FILE_VERSION) {
        std::cerr << "Invalid mesh file version, was " << version << " expected " << MESH_FILE_VERSION << ".\n";
        return false;
    }

    fread(&mesh.num_indices, sizeof(mesh.num_indices), 1, f);
    std::cout << "Number of indices " << mesh.num_indices << ".\n";
    mesh.indices = reinterpret_cast<decltype(mesh.indices)>(malloc(sizeof(*mesh.indices) * mesh.num_indices));
    fread(mesh.indices, sizeof(*mesh.indices), mesh.num_indices, f);

    fread(&mesh.attributes, sizeof(mesh.attributes), 1, f);

    fread(&mesh.num_vertices, sizeof(mesh.num_vertices), 1, f);
    std::cout << "Number of vertices " << mesh.num_vertices << ".\n";
    // @todo allocate in one go and index into buffer
    if (mesh.attributes & MESH_ATTRIBUTES_VERTICES) {
        mesh.vertices = reinterpret_cast<decltype(mesh.vertices)>(malloc(sizeof(*mesh.vertices) * mesh.num_vertices));
        fread(mesh.vertices, sizeof(*mesh.vertices), mesh.num_vertices, f);
    }
    if (mesh.attributes & MESH_ATTRIBUTES_NORMALS) {
        mesh.normals = reinterpret_cast<decltype(mesh.normals)>(malloc(sizeof(*mesh.normals) * mesh.num_vertices));
        fread(mesh.normals, sizeof(*mesh.normals), mesh.num_vertices, f);
    }
    if (mesh.attributes & MESH_ATTRIBUTES_TANGENTS) {
        mesh.tangents = reinterpret_cast<decltype(mesh.tangents)>(malloc(sizeof(*mesh.tangents) * mesh.num_vertices));
        fread(mesh.tangents, sizeof(*mesh.tangents), mesh.num_vertices, f);
    }
    if (mesh.attributes & MESH_ATTRIBUTES_UVS) {
        mesh.uvs = reinterpret_cast<decltype(mesh.uvs)>(malloc(sizeof(*mesh.uvs) * mesh.num_vertices));
        fread(mesh.uvs, sizeof(*mesh.uvs), mesh.num_vertices, f);
    }
    if (mesh.attributes & MESH_ATTRIBUTES_BONES) {
        mesh.bone_ids = reinterpret_cast<decltype(mesh.bone_ids)>(malloc(sizeof(*mesh.bone_ids) * mesh.num_vertices));
        fread(mesh.bone_ids, sizeof(*mesh.bone_ids), mesh.num_vertices, f);

        mesh.weights = reinterpret_cast<decltype(mesh.weights)>(malloc(sizeof(*mesh.weights) * mesh.num_vertices));
        fread(mesh.weights, sizeof(*mesh.weights), mesh.num_vertices, f);
        std::cout << "Loaded bone ids and weights.\n";
    }
    if (mesh.attributes & MESH_ATTRIBUTES_COLORS) {
        mesh.colors = reinterpret_cast<decltype(mesh.colors)>(malloc(sizeof(*mesh.colors) * mesh.num_vertices));
        fread(mesh.colors, sizeof(*mesh.colors), mesh.num_vertices, f);
    }

    // @note this doesn't support embedded materials for binary formats that assimp loads
    fread(&mesh.num_materials, sizeof(mesh.num_materials), 1, f);
    mesh.materials = new Material[mesh.num_materials];

    typedef std::tuple<Texture*, ImageData*> texture_imagedata_t;
    std::vector<texture_imagedata_t> texture_imagedata_list;
    for (int i = 0; i < mesh.num_materials; ++i) {
        auto& mat = mesh.materials[i];
        readMaterial(mat, assets, f);

        for (auto& p : mat.textures) {
            if (p.second && !p.second->complete) {
                auto img_data = new ImageData();
                texture_imagedata_list.emplace_back(texture_imagedata_t{ p.second, img_data });
            }
        }

        std::cout << "Material " << i << ": \n" << mat << "\n";
    }
    loadTextureImageDataList(texture_imagedata_list);

    fread(&mesh.num_submeshes, sizeof(mesh.num_submeshes), 1, f);
    mesh.material_indices = reinterpret_cast<decltype(mesh.material_indices)>(malloc(sizeof(*mesh.material_indices) * mesh.num_submeshes));
    mesh.draw_start = reinterpret_cast<decltype(mesh.draw_start)>(malloc(sizeof(*mesh.draw_start) * mesh.num_submeshes));
    mesh.draw_count = reinterpret_cast<decltype(mesh.draw_count)>(malloc(sizeof(*mesh.draw_count) * mesh.num_submeshes));
    mesh.transforms = reinterpret_cast<decltype(mesh.transforms)>(malloc(sizeof(*mesh.transforms) * mesh.num_submeshes));
    mesh.aabbs      = reinterpret_cast<decltype(mesh.aabbs     )>(malloc(sizeof(*mesh.aabbs     ) * (mesh.num_submeshes+1)));
    mesh.submesh_names = new std::string[mesh.num_submeshes];
    fread(mesh.material_indices, sizeof(*mesh.material_indices), mesh.num_submeshes, f);
    fread(mesh.draw_start, sizeof(*mesh.draw_start), mesh.num_submeshes, f);
    fread(mesh.draw_count, sizeof(*mesh.draw_count), mesh.num_submeshes, f);
    fread(mesh.transforms, sizeof(*mesh.transforms), mesh.num_submeshes, f);
    for (int i = 0; i < mesh.num_submeshes; i++) { readString(mesh.submesh_names[i], f); }

    // @todo serialize aabb
    for (uint64_t i = 0; i < mesh.num_submeshes; i++) {
        calculateAABB(mesh.aabbs[i], mesh.vertices, mesh.num_vertices, &mesh.indices[mesh.draw_start[i]], mesh.draw_count[i]);
    }
    calculateAABB(mesh.aabbs[mesh.num_submeshes], mesh.vertices, mesh.num_vertices, mesh.indices, mesh.num_indices);
}

constexpr uint16_t LEVEL_FILE_VERSION = 2U;
constexpr uint16_t LEVEL_FILE_MAGIC = 7123U;
void writeLevel(Level& level, FILE* f) {
    fwrite(&LEVEL_FILE_MAGIC, sizeof(LEVEL_FILE_MAGIC), 1, f);
    fwrite(&LEVEL_FILE_VERSION, sizeof(LEVEL_FILE_VERSION), 1, f);

    fwrite(&level.type, sizeof(level.type), 1, f);
    writeCamera(level.camera, f);
    writeEnvironment(level.environment, f);

    //
    // Write list of entities, replacing any assets with their handles
    //
    for (int i = 0; i < ENTITY_COUNT; i++) {
        auto e = level.entities.entities[i];
        if (e == nullptr || e->type == ENTITY) continue;

        fwrite(&e->type, sizeof(EntityType), 1, f);

        writeEntity(e, f);
        if (entityInherits(e->type, MESH_ENTITY)) {
            writeMeshEntity((MeshEntity*)e, f);
        }
        if (entityInherits(e->type, ANIMATED_MESH_ENTITY)) {
            writeAnimatedMeshEntity((AnimatedMeshEntity*)e, f);
        }
        if (entityInherits(e->type, WATER_ENTITY)) {
            writeWaterEntity((WaterEntity*)e, f);
        }
        if (entityInherits(e->type, COLLIDER_ENTITY)) {
            writeColliderEntity((ColliderEntity*)e, f);
        }
        if (entityInherits(e->type, PLAYER_ENTITY)) {
            writePlayerEntity((PlayerEntity*)e, f);
        }
    }
}

// @todo if needed make loading asign new ids such that connections are maintained
// @todo any entities that store ids must be resolved so that invalid ie wrong versions are NULLID 
// since we dont maintain version either, for now the no entity saves ids
bool readLevel(Level& level, AssetManager& assets, FILE* f) {
    std::remove_cv_t<decltype(LEVEL_FILE_MAGIC)> magic;
    fread(&magic, sizeof(magic), 1, f);
    if (magic != LEVEL_FILE_MAGIC) {
        std::cerr << "Invalid level file magic, was " << magic << " expected " << LEVEL_FILE_MAGIC << ".\n";
        return false;
    }

    std::remove_cv_t<decltype(LEVEL_FILE_VERSION)> version;
    fread(&version, sizeof(version), 1, f);
    if (version != LEVEL_FILE_VERSION) {
        std::cerr << "Invalid level file version, was " << version << " expected " << LEVEL_FILE_VERSION << ".\n";
        return false;
    }

    fread(&level.type, sizeof(level.type), 1, f);
    readCamera(level.camera, f);
    readEnvironment(level.environment, assets, f);


    std::vector<texture_imagedata_t> texture_imagedata_list; // Images for custom materials

    //
    // Write list of entities, replacing any assets with their handles
    //
    auto& entities = level.entities;
    entities.clear();
    char c;
    while ((c = fgetc(f)) != EOF) {
        ungetc(c, f);

        EntityType type;
        fread(&type, sizeof(EntityType), 1, f);

        auto e = allocateEntity(NULLID, type);
        readEntity(e, f);

        if (entityInherits(e->type, MESH_ENTITY)) {
            readMeshEntity((MeshEntity*)e, assets, texture_imagedata_list, f);
        }
        if (entityInherits(e->type, ANIMATED_MESH_ENTITY)) {
            readAnimatedMeshEntity((AnimatedMeshEntity*)e, assets, f);
        }
        if (entityInherits(e->type, WATER_ENTITY)) {
            readWaterEntity((WaterEntity*)e, f);
        }
        if (entityInherits(e->type, COLLIDER_ENTITY)) {
            readColliderEntity((ColliderEntity*)e, f);
        }
        if (entityInherits(e->type, PLAYER_ENTITY)) {
            readPlayerEntity((PlayerEntity*)e, f);
        }

        // Water and players are special cases since there can only be one per level
        bool unique = entityInherits(e->type, WATER_ENTITY) || entityInherits(e->type, PLAYER_ENTITY);
        if (unique) {
            if (entityInherits(e->type, PLAYER_ENTITY)) {
                if (entities.player != NULLID) {
                    std::cerr << "Duplicate player in level, skipping\n";
                    free(e);
                    continue;
                }
                else {
                    entities.setEntity(entities.getFreeId().i, e);
                    entities.player = e->id;
                }
            }
            else if (entityInherits(e->type, WATER_ENTITY)) {
                if (entities.water != NULLID) {
                    std::cerr << "Duplicate water in level, skipping\n";
                    free(e);
                    continue;
                }
                else {
                    entities.setEntity(entities.getFreeId().i, e);
                    entities.water = e->id;
                }
            }
        } else {
            // @todo preserve ids, you probably need to write free entity ids or
            // reorder to preserve relationships but remove gaps
            entities.setEntity(entities.getFreeId().i, e);
        }

        std::cout << "Loaded entity of type " << type << " with id " << e->id.i << ".\n";
    }

    loadTextureImageDataList(texture_imagedata_list);

    return true;
}