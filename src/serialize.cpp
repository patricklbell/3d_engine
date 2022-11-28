#include "serialize.hpp"
#include <stdlib.h>
#include "entities.hpp"
#include "globals.hpp"
#include "utilities.hpp"
#include "assets.hpp"

static void writeString(std::string_view str, FILE* f) {
    if(str.size() > (uint8_t)-1) {
        std::cerr << "Handle " << str<< " is too long\n";
    }
    uint8_t len = str.size();
    fwrite(&len, sizeof(len), 1, f);
    fwrite(str.data(), len, 1, f);
}

static void readString(std::string &str, FILE* f) {
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

void writeMeshEntity(MeshEntity* e, std::unordered_map<uint64_t, uint64_t> asset_lookup, FILE* f) {
    fwrite(&e->position, sizeof(e->position), 1, f);
    fwrite(&e->rotation, sizeof(e->rotation), 1, f);
    fwrite(&e->scale, sizeof(e->scale), 1, f);

    // @editor, it will be annoying but the level file will probably have to be translated for release
    fwrite(&e->gizmo_position_offset, sizeof(e->gizmo_position_offset), 1, f);

    uint64_t lookup = asset_lookup[reinterpret_cast<uint64_t>(e->mesh)];
    fwrite(&lookup, sizeof(lookup), 1, f);

    fwrite(&e->casts_shadow, sizeof(e->casts_shadow), 1, f);
    fwrite(&e->do_lightmap, sizeof(e->do_lightmap), 1, f);
    
    uint64_t num_overriden_materials = e->overidden_materials.size();
    fwrite(&num_overriden_materials, sizeof(num_overriden_materials), 1, f);
    for (const auto& p : e->overidden_materials) {
        fwrite(&p.first, sizeof(p.first), 1, f);
        writeMaterial(p.second, f);
    }
}
void readMeshEntity(MeshEntity* e, const std::unordered_map<uint64_t, void*> index_to_asset, FILE* f) {
    fread(&e->position, sizeof(e->position), 1, f);
    fread(&e->rotation, sizeof(e->rotation), 1, f);
    fread(&e->scale, sizeof(e->scale), 1, f);

    fread(&e->gizmo_position_offset, sizeof(e->gizmo_position_offset), 1, f);

    uint64_t lookup;
    fread(&lookup, sizeof(lookup), 1, f);
    auto lu = index_to_asset.find(lookup);
    if (lu != index_to_asset.end()) {
        e->mesh = (Mesh*)lu->second;
    }
    else {
        std::cerr << "Unknown mesh index " << lookup << " when reading mesh entity\n";
    }

    fseek(f, sizeof(glm::vec3), SEEK_CUR);
    fseek(f, 3*sizeof(float), SEEK_CUR);
    /*fread(&e->albedo_mult, sizeof(e->albedo_mult), 1, f);
    fread(&e->roughness_mult, sizeof(e->roughness_mult), 1, f);
    fread(&e->metal_mult, sizeof(e->metal_mult), 1, f);
    fread(&e->ao_mult, sizeof(e->ao_mult), 1, f);*/

    fread(&e->casts_shadow, sizeof(e->casts_shadow), 1, f);

    fread(&e->do_lightmap, sizeof(e->do_lightmap), 1, f);
    uint8_t lightmap_calculated;
    fread(&lightmap_calculated, sizeof(lightmap_calculated), 1, f);

    if (lightmap_calculated) {
        uint64_t lookup;
        fread(&lookup, sizeof(lookup), 1, f);

        /*auto lu = index_to_asset.find(lookup);
        if (lu != index_to_asset.end()) {
            e->lightmap = (Texture*)lu->second;
        }
        else {
            std::cerr << "Unknown lightmap texture index " << lookup << " when reading mesh entity\n";
        }*/
    }
}

void writeAnimatedMeshEntity(AnimatedMeshEntity* e, std::unordered_map<uint64_t, uint64_t> asset_lookup, FILE* f) {
    uint64_t lookup = asset_lookup[reinterpret_cast<uint64_t>(e->animesh)];
    fwrite(&lookup, sizeof(lookup), 1, f);

    fwrite(&e->default_event.current_time, sizeof(e->default_event.current_time), 1, f);
    fwrite(&e->default_event.time_scale, sizeof(e->default_event.time_scale), 1, f);
    fwrite(&e->default_event.loop, sizeof(e->default_event.loop), 1, f);
    fwrite(&e->default_event.playing, sizeof(e->default_event.playing), 1, f);

    if (e->default_event.animation != nullptr) {
        auto len = e->default_event.animation->name.size();
        if (len > 255 || len < 0) {
            std::cerr << "Animation " << e->default_event.animation->name << " longer than 255 or corrupted, truncating\n";
        }

        uint8_t nl = len;
        fwrite(&nl, sizeof(nl), 1, f);
        fwrite(e->default_event.animation->name.data(), nl, 1, f);
    }
    else {
        uint8_t nl = 0;
        fwrite(&nl, sizeof(nl), 1, f);
    }
}
void readAnimatedMeshEntity(AnimatedMeshEntity* e, const std::unordered_map<uint64_t, void*> index_to_asset, FILE* f) {
    uint64_t lookup;
    fread(&lookup, sizeof(lookup), 1, f);
    auto lu = index_to_asset.find(lookup);
    if (lu != index_to_asset.end()) {
        e->animesh = (AnimatedMesh*)lu->second;
    }
    else {
        std::cerr << "Unknown animated mesh index " << lookup << " when reading animated mesh entity\n";
    }

    fread(&e->default_event.current_time, sizeof(e->default_event.current_time), 1, f);
    fread(&e->default_event.time_scale, sizeof(e->default_event.time_scale), 1, f);
    fread(&e->default_event.loop, sizeof(e->default_event.loop), 1, f);
    fread(&e->default_event.playing, sizeof(e->default_event.playing), 1, f);

    uint8_t animation_name_len;
    fread(&animation_name_len, sizeof(animation_name_len), 1, f);
    if (animation_name_len > 0) {
        std::string animation_name;
        animation_name.resize(animation_name_len);
        fread(animation_name.data(), animation_name_len, 1, f);

        if (e->animesh != nullptr) {
            auto lu = e->animesh->name_animation_map.find(animation_name);
            if (lu != e->animesh->name_animation_map.end()) {
                e->default_event.animation = &lu->second;
            }
            else {
                std::cerr << "Unknown animation " << animation_name << " when reading animated mesh entity\n";
            }
        }
        else {
            std::cerr << "Animation name " << animation_name << " was set but animesh was null, something went wrong\n";
        }
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

void writeVegetationEntity(VegetationEntity* e, std::unordered_map<uint64_t, uint64_t> asset_lookup, FILE* f) {
    uint64_t lookup = asset_lookup[reinterpret_cast<uint64_t>(e->texture)];
    fwrite(&lookup, sizeof(lookup), 1, f);
}
void readVegetationEntity(VegetationEntity* e, const std::unordered_map<uint64_t, void*> index_to_asset, FILE* f) {
    uint64_t lookup;
    fread(&lookup, sizeof(lookup), 1, f);
    auto lu = index_to_asset.find(lookup);
    if (lu != index_to_asset.end()) {
        e->texture = (Texture*)lu->second;
    }
    else {
        std::cerr << "Unknown texture index " << lookup << " when reading vegetation entity\n";
    }
}

void writeCamera(const Camera& camera, FILE* f) {
    fwrite(&camera.position, sizeof(camera.position), 1, f);
    fwrite(&camera.target, sizeof(camera.target), 1, f);
}
void readCamera(Camera& camera, FILE* f) {
    fread(&camera.position, sizeof(camera.position), 1, f);
    fread(&camera.target, sizeof(camera.target), 1, f);

    camera.view_updated = true;
}

void writeMaterial(const Material& mat, FILE* f) {
    static const auto write_texture = [&f](Texture* tex) {
        uint64_t format = tex->format;
        fwrite(&format, sizeof(format), 1, f);

        char is_color = tex->is_color;
        fwrite(&is_color, sizeof(is_color), 1, f);

        if (tex->is_color) {
            fwrite(&tex->color, sizeof(tex->color), 1, f);
        }
        else {
            if (tex->handle.size() > (uint8_t)-1) {
                std::cerr << "Handle " << tex->handle << " is too long\n";
            }
            uint8_t len = tex->handle.size();
            fwrite(&len, sizeof(len), 1, f);
            fwrite(tex->handle.c_str(), len, 1, f);
        }
    };

    static const auto write_uniform = [&f](const Uniform &u) {
        fwrite(&u.type, sizeof(u.type), 1, f);
        uint64_t size = Uniform::size(u.type);
        fwrite(&size, sizeof(size), 1, f);
        fwrite(u.data, size, 1, f);
    };

    fwrite(&mat.type, sizeof(mat.type), 1, f);

    uint64_t num_textures = mat.textures.size();
    fwrite(&num_textures, sizeof(num_textures), 1, f);
    for (const auto& p : mat.textures) {
        if (p.second) {
            fwrite(&p.first, sizeof(p.first), 1, f);
            write_texture(p.second);
        }
    }

    uint64_t num_uniforms = mat.uniforms.size();
    fwrite(&num_uniforms, sizeof(num_uniforms), 1, f);
    for (const auto& p : mat.uniforms) {
        const auto& name = p.first;
        const auto& uniform = p.second;

        writeString(name, f);
        write_uniform(uniform);
    }
}

void readMaterial(Material& mat, AssetManager& asset_manager, FILE* f) {
    static const auto read_texture = [&f, &asset_manager]() {
        Texture* tex;

        uint64_t format;
        fread(&format, sizeof(format), 1, f);

        char is_color;
        fread(&is_color, sizeof(is_color), 1, f);
        
        if (is_color) {
            glm::vec4 color;
            fread(&color, sizeof(color), 1, f);
            tex = asset_manager.getColorTexture(color, format == GL_FALSE ? GL_RGBA : format);
        } else {
            uint8_t len;
            fread(&len, sizeof(len), 1, f);
            std::string path;
            path.resize(len);
            fread(&path[0], sizeof(char) * len, 1, f);

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

    static const auto read_uniform = [&f]() {
        Uniform::Type type;
        fread(&type, sizeof(type), 1, f);
        uint64_t size;
        fread(&size, sizeof(size), 1, f);

        void* data = malloc(size);
        fread(data, size, 1, f);

        return Uniform(data, type);
    };

    fread(&mat.type, sizeof(mat.type), 1, f);

    uint64_t num_textures;
    fread(&num_textures, sizeof(num_textures), 1, f);
    for (uint64_t i = 0; i < num_textures; i++) {
        TextureSlot slot;
        fread(&slot, sizeof(slot), 1, f);
        mat.textures[slot] = read_texture();
    }

    uint64_t num_uniforms;
    fread(&num_uniforms, sizeof(num_uniforms), 1, f);
    for (uint64_t i = 0; i < num_uniforms; i++) {
        std::string name;

        readString(name, f);
        mat.uniforms[name] = read_uniform();
    }
}

constexpr uint16_t MESH_FILE_VERSION = 4U;
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
        std::cout << "Loaded bone ids and weight.\n";
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

    fread(&mesh.num_submeshes, sizeof(mesh.num_submeshes), 1, f);

    mesh.material_indices = reinterpret_cast<decltype(mesh.material_indices)>(malloc(sizeof(*mesh.material_indices) * mesh.num_submeshes));
    mesh.draw_start = reinterpret_cast<decltype(mesh.draw_start)>(malloc(sizeof(*mesh.draw_start) * mesh.num_submeshes));
    mesh.draw_count = reinterpret_cast<decltype(mesh.draw_count)>(malloc(sizeof(*mesh.draw_count) * mesh.num_submeshes));
    mesh.transforms = reinterpret_cast<decltype(mesh.transforms)>(malloc(sizeof(*mesh.transforms) * mesh.num_submeshes));
    fread(mesh.material_indices, sizeof(*mesh.material_indices), mesh.num_submeshes, f);
    fread(mesh.draw_start, sizeof(*mesh.draw_start), mesh.num_submeshes, f);
    fread(mesh.draw_count, sizeof(*mesh.draw_count), mesh.num_submeshes, f);
    fread(mesh.transforms, sizeof(*mesh.transforms), mesh.num_submeshes, f);
}