#ifndef ASSETS_H
#define ASSETS_H

bool load_assimp(
	std::string path, 
	std::vector<unsigned short> & indices,
	std::vector<glm::vec3> & vertices,
	std::vector<glm::vec2> & uvs,
	std::vector<glm::vec3> & normals,
	std::vector<glm::vec3> & tangents
);
bool load_mtl(Material * mat, const std::string &path);
void load_asset(ModelAsset * asset, const std::string &objpath, const std::string &mtlpath);
struct Material {
    std::string name;
    float     albedo[3] = {1,1,1};
    float     diffuse[3]        = {1,1,1};
    float     specular[3]       = {1,1,1};
    float     transFilter[3]    = {1,1,1};
    float     dissolve          = 1.0;
    float     specExp       = 10;
    float     reflectSharp  = 60;
    float     opticDensity  = 1.0;
    GLuint    tAlbedo       = GL_FALSE;
    GLuint    tDiffuse      = GL_FALSE;
    GLuint    tNormal       = GL_FALSE;

} typedef Material;

struct ModelAsset {
    std::string name;
    GLuint     programID;
    Material * mat;
    GLuint     indices;
    GLuint 	   vertices;
    GLuint 	   uvs;
    GLuint 	   normals;
    GLuint 	   tangents;
    GLuint     vao;
    GLenum     drawMode;
    GLenum     drawType;
    GLint      drawStart;
    GLint      drawCount;
} typedef ModelAsset;



#endif
