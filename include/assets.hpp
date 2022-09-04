#ifndef ASSETS_HPP
#define ASSETS_HPP

#include <string>
#include <vector>
#include <list>
#include <unordered_map>

// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include <assimp/scene.h> 

#include "texture.hpp"

#ifdef _WINDOWS
#define NOMINMAX 
#endif
#include <soloud.h>
#include <soloud_thread.h>
#include <soloud_wav.h>
#include <soloud_wavstream.h>

struct Material;
struct Mesh {
    std::string handle;

    // Serialised Section
    unsigned int    num_indices = 0;
    unsigned int   *indices = nullptr;

    // @note Mesh is not responsible for material's pointers
    uint64_t        num_materials = 0;
    Material       *materials = nullptr;
    uint64_t       *material_indices = nullptr;

    uint64_t        num_vertices = 0;
    glm::fvec3*     vertices; 
    glm::fvec3*     normals;
    glm::fvec3*     tangents;
    glm::fvec2*     uvs;

    bool            transparent = false;

    uint64_t        num_meshes = 0;
    glm::mat4x4*    transforms = nullptr;
    GLenum          draw_mode = GL_TRIANGLES;
    GLenum          draw_type = GL_UNSIGNED_INT;
    GLint          *draw_start = nullptr;
    GLint          *draw_count = nullptr;

    GLuint          indices_vbo;
    GLuint 	        vertices_vbo;
    GLuint 	        uvs_vbo;
    GLuint 	        normals_vbo;
    GLuint 	        tangents_vbo;
    GLuint          vao;

    ~Mesh();
};

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
    std::unordered_map<std::string, Mesh>    handle_mesh_map;
    std::unordered_map<std::string, Texture> handle_texture_map;
    std::unordered_map<std::string, Audio>   handle_audio_map;
    std::unordered_map<glm::vec3,   Texture> color_texture_map;

    // @note that this function could cause you to "lose" a mesh if the path is the same
    Mesh* createMesh(const std::string &handle);
    // Loads mesh from file path, flag is_mesh determines whether to treat path as .mesh
    bool loadMesh(Mesh *mesh, const std::string &path, const bool is_mesh=false);
    bool loadMeshAssimp(Mesh *mesh, const std::string &path);
    bool loadMeshFile(Mesh *mesh, const std::string &path);
    static bool writeMeshFile(const Mesh *mesh, const std::string &path);

    //bool loadMtl(std::unordered_map<std::string, Material *> &material_map, const std::string &path);
    //bool loadAssetObj(Mesh *asset, std::string objpath, std::string mtlpath);

    // @note that this function could cause you to "lose" a texture if the path is the same
    Texture* createTexture(const std::string &handle);
    bool loadTextureFromAssimp(Texture *tex, aiMaterial* mat, const aiScene* scene, aiTextureType texture_type, GLint internal_format=GL_SRGB);
    static bool loadTexture(Texture *tex, const std::string &path, GLint internal_format=GL_SRGB);
    static bool loadCubemapTexture(Texture *tex, const std::array<std::string,FACE_NUM_FACES> &paths, GLint internal_format=GL_SRGB);

    Audio* createAudio(const std::string& handle);

    // This returns nullptr if the asset doesn't exist
    Mesh*    getMesh(const std::string &path);
    Texture* getTexture(const std::string &path);
    Texture* getColorTexture(const glm::vec3 &col, GLint internal_format=GL_RGB);

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

#endif
