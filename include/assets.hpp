#ifndef ASSETS_HPP
#define ASSETS_HPP

#include <string>
#include <vector>
#include <unordered_map>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include <assimp/scene.h> 

struct Material {
    std::string name;
// Store constant uniforms in 1x1 textures, it would be good to
// check if this is performant.
    GLuint    t_normal               = 0;
    GLuint    t_albedo               = 0;
    GLuint    t_ambient              = 0;
    GLuint    t_roughness            = 0;
    GLuint    t_metallic             = 0;
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
};

struct Asset {
    AssetType type = AssetType::ASSET;
    std::string path;
    Asset(std::string _path) : path(_path){}
};

bool loadAsset(Asset *asset);

struct MeshAsset : Asset {
    Mesh mesh;
    MeshAsset(std::string _path) : Asset(_path){
        type = (AssetType)(type | AssetType::MESH_ASSET);
    };
    ~MeshAsset();
};
bool loadMesh(Mesh &mesh, std::string path);

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
GLuint loadTextureFromAssimp(aiMaterial *mat, const aiScene *scene, aiTextureType texture_type, GLint internal_format);
#endif
