#ifndef ASSETS_HPP
#define ASSETS_HPP

#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <set>

// Include GLEW
#include <GL/glew.h>
// Include GLFW
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <assimp/scene.h> 

#ifdef _WINDOWS
#define NOMINMAX 
#endif
#include <soloud.h>
#include <soloud_thread.h>
#include <soloud_wav.h>
#include <soloud_wavstream.h>

#include <texture.hpp>

enum AssetType : char {
    MESH_ASSET = 0,
    ANIMATED_MESH_ASSET,
    TEXTURE_ASSET,
    COLOR_ASSET,
    AUDIO_ASSET,
    NONE_ASSET,
};

struct Material;

enum MeshAttributes : char {
    MESH_ATTRIBUTES_NONE        = 0,
    MESH_ATTRIBUTES_VERTICES    = 1 << 0,
    MESH_ATTRIBUTES_NORMALS     = 1 << 1,
    MESH_ATTRIBUTES_TANGENTS    = 1 << 2,
    MESH_ATTRIBUTES_UVS         = 1 << 3,
    MESH_ATTRIBUTES_BONES       = 1 << 4,
    MESH_ATTRIBUTES_COLORS      = 1 << 5,
};

struct Mesh {
    bool complete = false;
    std::string handle;

    MeshAttributes attributes = MESH_ATTRIBUTES_NONE;

    unsigned int    num_indices = 0;
    unsigned int*   indices = nullptr;

    uint64_t        num_materials = 0;
    Material*       materials = nullptr; // @note !!!IMPORTANT!!! allocated with new
    uint64_t*       material_indices = nullptr;

    uint64_t        num_vertices = 0;
    glm::fvec3*     vertices = nullptr;
    glm::fvec3*     normals = nullptr;
    glm::fvec3*     tangents = nullptr;
    glm::fvec2*     uvs = nullptr;
    glm::fvec4*     colors = nullptr;
    glm::ivec4*     bone_ids = nullptr; // Indices of bones which affect each vertex
    glm::fvec4*     weights = nullptr; // Weights for each bone

    bool            transparent = false;

    // A submesh is an indice group which is used to avoid rebinding VAOs and duplicating vertices
    uint64_t        num_submeshes = 0;
    glm::mat4x4*    transforms = nullptr;
    GLint*          draw_start = nullptr;
    GLint*          draw_count = nullptr;

    GLenum          draw_mode = GL_TRIANGLES;
    GLenum          draw_type = GL_UNSIGNED_INT; // For now these won't work, but in future this could save memory

    GLuint          indices_vbo     = GL_FALSE;
    GLuint 	        vertices_vbo    = GL_FALSE;
    GLuint 	        normals_vbo     = GL_FALSE;
    GLuint 	        uvs_vbo         = GL_FALSE;
    GLuint 	        colors_vbo      = GL_FALSE;
    GLuint 	        tangents_vbo    = GL_FALSE;
    GLuint          bone_ids_vbo    = GL_FALSE;
    GLuint          weights_vbo     = GL_FALSE;
    GLuint          vao             = GL_FALSE;

    // Culling information @todo
    struct AABB {
        glm::vec4 data; // min x, min y, max x, max y
    };
    AABB* aabbs = nullptr;

    ~Mesh();
};

#define MAX_BONE_WEIGHTS 4
// @todo in loading you could limit loaded bone rather than in tick step
#define MAX_BONES 1000

struct AnimatedMesh {
    bool complete = false;
    std::string handle;

    ~AnimatedMesh(); // Frees C arrays

    uint64_t num_bones = 0;
    std::vector<glm::mat4> bone_offsets; // This could be c array, but requires some work
#define MAX_BONE_NAME_LENGTH 32
    std::vector<std::array<char, MAX_BONE_NAME_LENGTH>> bone_names; // @debug

    // @note this is baked into mesh transforms
    glm::mat4x4 global_transform; // Convert from model space to world space

    struct BoneKeyframes {
        uint64_t id;

        struct Keys {
            int32_t num_keys = 0;
            float* times = nullptr;

            // Speeds up finding the next keyframe, updated per tick
            int32_t prev_key_i = 0;
        };

        Keys position_keys;
        glm::vec3* positions = nullptr;

        Keys rotation_keys;
        glm::quat* rotations = nullptr;

        Keys scale_keys;
        glm::vec3* scales = nullptr;
    };

    struct BoneNode {
        uint64_t id = -1;

        glm::mat4 local_transform = glm::mat4(1.0f);

        int32_t parent_index = -1;
    };

    // Updated per frame for traversing tree
    struct BoneNodeAnimation {
        glm::mat4 global_transform;
        // There is probably a better way to apply transforms with blending @todo
        glm::mat4 global_transform_blended; // Used when traversing the tree
    };

    // Flattened tree which describes Bone relationships
    // needed since transformations are relative to parent
    std::vector<BoneNode> bone_node_list;
    std::vector<BoneNodeAnimation> bone_node_animation_list;

    struct Animation {
        std::string name;
        float duration;
        uint64_t ticks_per_second;

        uint64_t       num_bone_keyframes;
        BoneKeyframes* bone_keyframes = nullptr;

        std::unordered_map<uint64_t, uint64_t> bone_id_keyframe_index_map;
    };

    std::unordered_map<std::string, Animation> name_animation_map;
};
float getAnimationDuration(const AnimatedMesh &animesh, const std::string& name);

glm::mat4x4 tickBonesKeyframe(AnimatedMesh::BoneKeyframes& keyframes, float time, bool looping);

struct Texture {
    bool complete = false;
    std::string handle;

    GLuint id = GL_FALSE;
    GLint format = GL_RGBA;
    glm::ivec2 resolution;

    // @debug
    // just used by editor (for now, probably check)
    bool is_color = false;
    glm::vec4 color;
};

struct Audio {
    bool complete = false;
    std::string handle;

    SoLoud::WavStream wav_stream;
};

inline void hash_combine(std::size_t& seed) { }

template <typename T, typename... Rest>
inline void hash_combine(std::size_t& seed, const T& v, Rest... rest) {
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    hash_combine(seed, rest...);
}

// Define hash for vec4 so AssetManager can be default initialized
namespace std {
    template <typename T, glm::precision P>
    struct hash<glm::tvec4<T, P>>
    {
        std::size_t operator()(const glm::tvec4<T, P>& v) const
        {
            std::size_t h;
            hash_combine(h, v.x, v.y, v.z, v.w);
            return h;
        }
    };
}

struct AssetManager {
    std::unordered_map<std::string, Mesh>           handle_mesh_map;
    std::unordered_map<std::string, AnimatedMesh>   handle_animated_mesh_map;
    std::unordered_map<std::string, Texture>        handle_texture_map;
    std::unordered_map<std::string, Audio>          handle_audio_map;
    std::unordered_map<glm::vec4,   Texture>        color_texture_map;

    // @note that this function could cause you to "lose" a mesh if the path is the same
    Mesh* createMesh(const std::string &handle);
    bool loadMeshAssimp(Mesh *mesh, const std::string &path);
    bool loadMeshFile(Mesh *mesh, const std::string &path);
    static bool writeMeshFile(const Mesh *mesh, const std::string &path);
    bool loadMeshAssimpScene(Mesh* mesh, const std::string& path, const aiScene* scene, 
        const std::vector<aiMesh*>& ai_meshes, const std::vector<aiMatrix4x4>& ai_meshes_global_transforms);

    // Loads both since most animation formats combine animation and mesh info
    bool loadAnimatedMeshAssimp(AnimatedMesh* animesh, Mesh *mesh, const std::string& path);

    AnimatedMesh* createAnimatedMesh(const std::string& handle);
    // Note that these don't load/save any mesh or bone weights
    static bool loadAnimationFile(AnimatedMesh* animesh, const std::string& path);
    static bool writeAnimationFile(const AnimatedMesh* animesh, const std::string& path);

    //bool loadMtl(std::unordered_map<std::string, Material *> &material_map, const std::string &path);
    //bool loadAssetObj(Mesh *asset, std::string objpath, std::string mtlpath);

    // @note that this function could cause you to "lose" a texture if the path is the same
    Texture* createTexture(const std::string &handle);
    bool loadTextureFromAssimp(Texture *&tex, aiMaterial* mat, const aiScene* scene, aiTextureType texture_type, GLint internal_format=GL_RGBA, bool floating = false);
    static bool loadTexture(Texture *tex, const std::string &path, GLenum format=GL_RGBA, const GLint wrap = GL_REPEAT, bool floating = false, bool trilinear = true);
    static bool loadCubemapTexture(Texture *tex, const std::array<std::string, FACE_NUM_FACES> &paths, GLenum format = GL_RGBA, const GLint wrap = GL_REPEAT, bool floating = false, bool trilinear = true);

    Audio* createAudio(const std::string& handle);

    // This returns nullptr if the asset doesn't exist
    Mesh*           getMesh(const std::string &path) const;
    AnimatedMesh*   getAnimatedMesh(const std::string& path) const;
    Texture*        getTexture(const std::string &path) const;
    Texture*        getColorTexture(const glm::vec4 &col, GLint format=GL_RGB);

    void clearExcluding(const std::set<std::string> &excluded);
    void clear();
};

namespace MaterialType {
    enum Type : uint64_t {
        NONE        = 0,
        PBR         = 1 << 0,
        BLINN_PHONG = 1 << 1,
        EMISSIVE    = 1 << 2,
        METALLIC    = 1 << 3,
        ALPHA_CLIP  = 1 << 4,
        VEGETATION  = 1 << 5,
        LIGHTMAPPED = 1 << 6,
        AO          = 1 << 7,
    };
}

enum class TextureSlot : uint64_t {
    NORMAL      = 1,
    ALPHA_CLIP  = 0,

    // PBR
    ALBEDO      = 0,
    METAL       = 2,
    ROUGHNESS   = 3,

    // BLINN PHONG
    DIFFUSE     = 0,
    SPECULAR    = 2,
    SHININESS   = 3,

    AO          = 4,
    GI          = 4,

    EMISSIVE    = 4,
};

struct Uniform {
    enum class Type: uint64_t {
        NONE = 0,
        VEC4,
        VEC3,
        VEC2,
        FLOAT,
        INT,
    };

    Uniform(glm::vec4 val) {
        type = Type::VEC4;
        data = malloc(size(type));
        new (data) glm::vec4(val);
    }
    Uniform(glm::vec3 val) {
        type = Type::VEC3;
        data = malloc(size(type));
        new (data) glm::vec3(val);
    }
    Uniform(glm::vec2 val) {
        type = Type::VEC2;
        data = malloc(size(type));
        new (data) glm::vec2(val);
    }
    Uniform(float val) {
        type = Type::FLOAT;
        data = malloc(size(type));
        new (data) GLfloat(val);
    }
    Uniform(int val) {
        type = Type::INT;
        data = malloc(size(type));
        new (data) GLint(val);
    }
    Uniform(void* _data, Type _type) 
        : data(_data), type(_type) 
    {
    }
    Uniform()
        : data(nullptr), type(Type::NONE) // Use placement new so we can also pass a pre allocated buffer
    {
    }

    void bind(GLint loc) const {
        switch (type)
        {
        case Uniform::Type::VEC4:
            glUniform4fv(loc, 1, (GLfloat*)data);
            break;
        case Uniform::Type::VEC3:
            glUniform3fv(loc, 1, (GLfloat*)data);
            break;
        case Uniform::Type::VEC2:
            glUniform2fv(loc, 1, (GLfloat*)data);
            break;
        case Uniform::Type::FLOAT:
            glUniform1fv(loc, 1, (GLfloat*)data);
            break;
        case Uniform::Type::INT:
            glUniform1iv(loc, 1, (GLint*)data);
            break;
        default:
            break;
        }
    }

    constexpr static size_t size(Type t) {
        switch (t)
        {
        case Type::VEC4:   return sizeof(glm::vec4);
        case Type::VEC3:   return sizeof(glm::vec3);
        case Type::VEC2:   return sizeof(glm::vec2);
        case Type::FLOAT:  return sizeof(GLfloat);
        case Type::INT:    return sizeof(GLint);
        default:                    return 0;
        }
    }

    ~Uniform() {
        free(data);
    }

    void* data = nullptr;
    Type type = Type::NONE;
};

struct Material {
    MaterialType::Type type = MaterialType::NONE;
    std::unordered_map<TextureSlot, Texture*> textures;
    std::unordered_map<std::string, Uniform> uniforms; // @note this string could be made into a binding location

    ~Material();
};

extern Material* default_material;
void initDefaultMaterial(AssetManager &asset_manager);

std::ostream &operator<<(std::ostream &os, const Texture &t);
std::ostream &operator<<(std::ostream &os, const Material &m);
#endif
