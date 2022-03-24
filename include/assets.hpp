#ifndef ASSETS_HPP
#define ASSETS_HPP

#include <string>
#include <vector>
#include <unordered_map>

#include <glm/glm.hpp>

#include <assimp/scene.h> 

#include <GLFW/glfw3.h>

struct Material {
    std::string name;
    float     ambient[3]       = {1,1,1};
    float     diffuse[3]       = {1,1,1};
    float     specular[3]      = {1,1,1};
    float     trans_filter[3]  = {1,1,1};
    float     dissolve         = 1.0;
    float     spec_exp         = 10;
    float     spec_int         = 10;
    float     reflect_sharp    = 60;
    float     optic_density    = 1.0;
    GLuint    t_ambient        = 0;
    GLuint    t_diffuse        = 0;
    GLuint    t_normal         = 0;

} typedef Material;

extern Material *default_material;

void initDefaultMaterial();

struct Mesh {
    std::string name;
    int        num_materials;
    Material **materials;
    GLuint     indices;
    GLuint 	   vertices;
    GLuint 	   uvs;
    GLuint 	   normals;
    GLuint 	   tangents;
    GLuint     vao;
    GLenum     draw_mode;
    GLenum     draw_type;
    GLint     *draw_start;
    GLint     *draw_count;
} typedef Mesh;

bool loadAssimp(
	std::string path,
	std::vector<std::pair<unsigned int, unsigned int>> & mesh_ranges,
	std::vector<std::string> & mesh_materials,
	std::vector<unsigned short> & indices,
	std::vector<glm::vec3> & vertices,
	std::vector<glm::vec2> & uvs,
	std::vector<glm::vec3> & normals,
	std::vector<glm::vec3> & tangents
);
bool loadMtl(std::unordered_map<std::string, Material *> &material_map, const std::string &path);
bool loadAssetObj(Mesh *asset, const std::string &objpath, const std::string &mtlpath);
bool loadAsset(Mesh *asset, const std::string &path);
GLuint loadTextureFromAssimp(aiMaterial *mat, aiTextureType texture_type);
#endif
