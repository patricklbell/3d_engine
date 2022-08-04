#ifndef ASSETS_HPP
#define ASSETS_HPP

#include <map>
#include <string>
#include <vector>
#include <unordered_map>

// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include <assimp/scene.h> 

struct TextureAsset;

struct Material {
// Store constant uniforms in 1x1 textures, it would be good to
// check if this is performant.
    TextureAsset *t_normal    = nullptr;
    TextureAsset *t_albedo    = nullptr;
    TextureAsset *t_ambient   = nullptr;
    TextureAsset *t_roughness = nullptr;
    TextureAsset *t_metallic  = nullptr;
} typedef Material;

extern Material *default_material;

void initDefaultMaterial();

struct Mesh {
    unsigned short *indices;
    int             num_materials = 0;
    int             num_vertices = 0;
    int             num_indices = 0;
    bool            transparent = false;
    Material      **materials;
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
    GLint          *draw_start;
    GLint          *draw_count;
} typedef Mesh;

enum AssetType {
    ASSET = 0,
    MESH_ASSET = 1,
    TEXTURE_ASSET = 2,
    CUBEMAP_ASSET = 4,
};

struct Asset {
    AssetType type = AssetType::ASSET;
    std::string path;
    Asset(std::string _path) : path(_path){}
};

struct MeshAsset : Asset {
    Mesh mesh;
    MeshAsset(std::string _path) : Asset(_path){
        type = (AssetType)(type | AssetType::MESH_ASSET);
    };
    ~MeshAsset();
};
bool loadMesh(Mesh &mesh, const std::string &path, std::map<std::string, Asset*> &assets);
bool writeMeshFile(const Mesh &mesh, std::string path);
bool readMeshFile(std::map<std::string, Asset*> &assets, Mesh &mesh, std::string path);

struct TextureAsset : Asset {
    GLuint texture_id;
    TextureAsset(std::string _path) : Asset(_path){
        type = (AssetType)(type | AssetType::TEXTURE_ASSET);
    };
    ~TextureAsset(){
        glDeleteTextures(1, &texture_id);
    }
};
TextureAsset *loadTextureFromAssimp(std::map<std::string, Asset*> &assets, aiMaterial *mat, const aiScene *scene, aiTextureType texture_type, GLint internal_format);
TextureAsset *createTextureAsset(std::map<std::string, Asset*> &assets, std::string path, GLint internal_format=GL_SRGB);

enum CubemapFaces : unsigned int {
    FACE_RIGHT = 0,
    FACE_LEFT, 
    FACE_TOP,
    FACE_BOTTOM,
    FACE_FRONT,
    FACE_BACK,
    FACE_NUM_FACES,
};

struct CubemapAsset: Asset {
    GLuint texture_id;

    CubemapAsset(std::string _path) : Asset(_path){
        type = (AssetType)(type | AssetType::CUBEMAP_ASSET);
    };
    ~CubemapAsset(){
        glDeleteTextures(1, &texture_id);
    }
};

CubemapAsset *createCubemapAsset(std::map<std::string, Asset*> &assets, std::array<std::string,FACE_NUM_FACES> paths, GLint internal_format=GL_RGB);

//bool loadAssimp(
//	std::string path,
//	std::vector<std::pair<unsigned int, unsigned int>> & mesh_ranges,
//	std::vector<std::string> & mesh_materials,
//	std::vector<unsigned short> & indices,
//	std::vector<glm::vec3> & vertices,
//	std::vector<glm::vec2> & uvs,
//	std::vector<glm::vec3> & normals,
//	std::vector<glm::vec3> & tangents,
//	std::vector<glm::vec3> & bitangents
//);
//bool loadMtl(std::unordered_map<std::string, Material *> &material_map, const std::string &path);
//bool loadAssetObj(Mesh *asset, const std::string &objpath, const std::string &mtlpath);
#endif
