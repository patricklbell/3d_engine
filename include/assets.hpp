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

struct Material;
struct Mesh {
    std::string handle;

    unsigned short *indices = nullptr;
    int             num_materials = 0;
    int             num_vertices = 0;
    int             num_indices = 0;
    bool            transparent = false;
    // @note Mesh is not responsible for material's pointers
    Material       *materials = nullptr;
    glm::fvec3*     vertices; 
    glm::fvec3*     normals;
    glm::fvec3*     tangents;
    glm::fvec2*     uvs;
    GLuint          indices_vbo;
    GLuint 	        vertices_vbo;
    GLuint 	        uvs_vbo;
    GLuint 	        normals_vbo;
    GLuint 	        tangents_vbo;
    GLuint          vao;
    GLenum          draw_mode;
    GLenum          draw_type;
    GLint          *draw_start = nullptr;
    GLint          *draw_count = nullptr;

    ~Mesh();
};

struct Texture {
    std::string handle;
    GLuint id;
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
    std::unordered_map<glm::vec3,   Texture> color_texture_map;

    // @note that this function could cause you to "lose" a mesh if the path is the same
    Mesh* createMesh(const std::string &handle);
    // Loads mesh from file path, flag is_mesh determines whether to treat path as .mesh
    bool loadMesh(Mesh *mesh, const std::string &path, const bool is_mesh=false);
    bool loadMeshAssimp(Mesh *mesh, const std::string &path);
    bool loadMeshFile(Mesh *mesh, const std::string &path);
    static bool writeMeshFile(const Mesh *mesh, const std::string &path);

    //bool loadMtl(std::unordered_map<std::string, Material *> &material_map, const std::string &path);
    //bool loadAssetObj(Mesh *asset, const std::string &objpath, const std::string &mtlpath);

    // @note that this function could cause you to "lose" a texture if the path is the same
    Texture* createTexture(const std::string &handle);
    static bool loadTextureFromAssimp(Texture *tex, aiMaterial* mat, const aiScene* scene, aiTextureType texture_type, GLint internal_format=GL_SRGB);
    static bool loadTexture(Texture *tex, const std::string &path, GLint internal_format=GL_SRGB);
    static bool loadCubemapTexture(Texture *tex, const std::array<std::string,FACE_NUM_FACES> &paths, GLint internal_format=GL_SRGB);

    // This returns nullptr if the asset doesn't exist
    Mesh*    getMesh(const std::string &path);
    Texture* getTexture(const std::string &path);
    Texture* getColorTexture(const glm::vec3 &col);

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
