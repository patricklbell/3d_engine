#ifndef ASSETS_HPP
#define ASSETS_HPP

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include <GLFW/glfw3.h>

struct Material {
    std::string name;
    float     albedo[3] = {1,1,1};
    float     diffuse[3]        = {1,1,1};
    float     specular[3]       = {1,1,1};
    float     trans_filter[3]    = {1,1,1};
    float     dissolve          = 1.0;
    float     spec_exp       = 10;
    float     reflect_sharp  = 60;
    float     optic_density  = 1.0;
    GLuint    t_albedo       = GL_FALSE;
    GLuint    t_diffuse      = GL_FALSE;
    GLuint    t_normal       = GL_FALSE;

} typedef Material;

struct ModelAsset {
    std::string name;
    GLuint     program_id;
    Material * mat;
    GLuint     indices;
    GLuint 	   vertices;
    GLuint 	   uvs;
    GLuint 	   normals;
    GLuint 	   tangents;
    GLuint     vao;
    GLenum     draw_mode;
    GLenum     draw_type;
    GLint      draw_start;
    GLint      draw_count;
} typedef ModelAsset;

bool loadAssimp(
	std::string path, 
	std::vector<unsigned short> & indices,
	std::vector<glm::vec3> & vertices,
	std::vector<glm::vec2> & uvs,
	std::vector<glm::vec3> & normals,
	std::vector<glm::vec3> & tangents
);
bool loadMtl(Material *mat, const std::string &path);
void loadAsset(ModelAsset *asset, const std::string &objpath, const std::string &mtlpath);

#endif
