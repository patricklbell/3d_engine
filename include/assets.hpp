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

#include "texture.hpp"

#ifdef _WINDOWS
#define NOMINMAX 
#endif
#include <soloud.h>
#include <soloud_thread.h>
#include <soloud_wav.h>
#include <soloud_wavstream.h>

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
};

struct Mesh {
    std::string handle;

    MeshAttributes attributes = MESH_ATTRIBUTES_NONE;

    // Serialised Section
    unsigned int    num_indices = 0;
    unsigned int*   indices = nullptr;

    // @note Mesh is not responsible for material's pointers
    uint64_t        num_materials = 0;
    Material*       materials = nullptr;
    uint64_t*       material_indices = nullptr;

    uint64_t        num_vertices = 0;
    glm::fvec3*     vertices = nullptr;
    glm::fvec3*     normals = nullptr;
    glm::fvec3*     tangents = nullptr;
    glm::fvec2*     uvs = nullptr;
    glm::ivec4*     bone_ids = nullptr; // Indices of bones which affect each vertex
    glm::fvec4*     weights = nullptr; // Weights for each bone

    bool            transparent = false;

    uint64_t        num_meshes = 0;
    glm::mat4x4*    transforms = nullptr;
    GLenum          draw_mode = GL_TRIANGLES;
    GLenum          draw_type = GL_UNSIGNED_INT;
    GLint*          draw_start = nullptr;
    GLint*          draw_count = nullptr;

    GLuint          indices_vbo     = GL_FALSE;
    GLuint 	        vertices_vbo    = GL_FALSE;
    GLuint 	        uvs_vbo         = GL_FALSE;
    GLuint 	        normals_vbo     = GL_FALSE;
    GLuint 	        tangents_vbo    = GL_FALSE;
    GLuint          bone_ids_vbo    = GL_FALSE;
    GLuint          weights_vbo     = GL_FALSE;
    GLuint          vao             = GL_FALSE;

    ~Mesh();
};

#define MAX_BONE_WEIGHTS 4
// @todo in loading you could limit loaded bone rather than in tick step
#define MAX_BONES 100

struct AnimatedMesh {
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

        // Updated per tick
        glm::mat4 local_transformation;

        struct KeyPosition {
            glm::fvec3 position;
            float time;
        };
        struct KeyRotation {
            glm::fquat rotation;
            float time;
        };
        struct KeyScale {
            glm::fvec3 scale;
            float time;
        };

        int32_t num_position_keys;
        int32_t num_rotation_keys;
        int32_t num_scale_keys;
        KeyPosition *position_keys = nullptr;
        KeyRotation *rotation_keys = nullptr;
        KeyScale    *scale_keys    = nullptr;

        // Speeds up finding the next keyframe
        int32_t prev_position_key = 0;
        int32_t prev_rotation_key = 0;
        int32_t prev_scale_key    = 0;
    };

    struct BoneNode {
        uint64_t id = -1;

        glm::mat4 local_transform = glm::mat4(1.0f);
        glm::mat4 global_transform; // Used when traversing the tree
        int32_t parent_index = -1;
    };
    // Flattened tree which describes Bone relationships
    // needed since transformations are relative to parent
    std::vector<BoneNode> bone_node_list;

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

void tickBonesKeyframe(AnimatedMesh::BoneKeyframes& keyframes, float time, bool looping);

struct Texture {
    std::string handle;
    GLuint id = GL_FALSE;

    // @debug
    // just used by editor (for now, probably check)
    bool is_color = false;
    glm::vec3 color;

};

struct Audio {
    std::string handle;
    SoLoud::WavStream wav_stream;
};

//enum AssetType {
//    ASSET = 0,
//    MESH_ASSET = 1,
//    TEXTURE_ASSET = 2,
//};
//
//struct Asset {
//    AssetType type = AssetType::ASSET;
//    std::string path;
//    Asset(std::string _path) : path(_path){}
//};
//
//struct MeshAsset : Asset {
//    Mesh mesh;
//    MeshAsset(std::string _path) : Asset(_path){
//        type = AssetType::MESH_ASSET;
//    };
//    ~MeshAsset();
//};
//
//struct TextureAsset : Asset {
//    GLuint texture_id;
//    TextureAsset(std::string _path) : Asset(_path){
//        type = AssetType::TEXTURE_ASSET;
//    };
//    ~TextureAsset(){
//        glDeleteTextures(1, &texture_id);
//    }
//};

// Define hash for vec3 so AssetManager can be default initialized
namespace std {
    template <typename T, glm::precision P>
    struct hash<glm::tvec3<T, P>>
    {
        std::size_t operator()(const glm::tvec3<T, P>& k) const
        {
            using std::size_t;
            using std::hash;
            using std::string;

            // Compute individual hash values for first,
            // second and third and combine them using XOR
            // and bit shifting:

            return ((hash<T>()(k.x)
                ^ (hash<T>()(k.y) << 1)) >> 1)
                ^ (hash<T>()(k.z) << 1);
        }
    };
}

struct AssetManager {
    std::unordered_map<std::string, Mesh>           handle_mesh_map;
    std::unordered_map<std::string, AnimatedMesh>   handle_animated_mesh_map;
    std::unordered_map<std::string, Texture>        handle_texture_map;
    std::unordered_map<std::string, Audio>          handle_audio_map;
    std::unordered_map<glm::vec3,   Texture>        color_texture_map;

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
    bool loadTextureFromAssimp(Texture *&tex, aiMaterial* mat, const aiScene* scene, aiTextureType texture_type, GLint internal_format=GL_RGB16F);
    static bool loadTexture(Texture *tex, const std::string &path, GLint internal_format=GL_RGB16F);
    static bool loadCubemapTexture(Texture *tex, const std::array<std::string, FACE_NUM_FACES> &paths, GLint internal_format=GL_RGB16F);

    Audio* createAudio(const std::string& handle);

    // This returns nullptr if the asset doesn't exist
    Mesh*           getMesh(const std::string &path);
    AnimatedMesh*   getAnimatedMesh(const std::string& path);
    Texture*        getTexture(const std::string &path);
    Texture*        getColorTexture(const glm::vec3 &col, GLint internal_format=GL_RGB);

    void clearExcluding(const std::set<std::string> &excluded);
    void clear();
};

struct Material {
// Store constant uniforms in 1x1 textures, it would be good to
// check if this is performant.
    Texture* t_normal    = nullptr;
    Texture* t_albedo    = nullptr;
    Texture* t_ambient   = nullptr;
    Texture* t_roughness = nullptr;
    Texture* t_metallic  = nullptr;
};
extern Material* default_material;
void initDefaultMaterial(AssetManager &asset_manager);

std::ostream &operator<<(std::ostream &os, const Texture &t);
std::ostream &operator<<(std::ostream &os, const Material &m);
#endif
