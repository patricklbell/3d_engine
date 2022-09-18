#ifndef SHADER_HPP
#define SHADER_HPP

#include <string>
#include <unordered_map>

#include <GLFW/glfw3.h>

struct Shader {
    std::string handle = "";
    std::unordered_map<std::string, GLuint> uniforms;
    GLuint program = GL_FALSE;
    ~Shader();
};

bool loadShader(Shader& shader, std::string_view vertex_fragment_file_path, std::string_view macros, bool geometry);
void initGlobalShaders();
void updateGlobalShaders();

namespace shader {
    extern Shader animated_null, null, unified, animated_unified, water, gaussian_blur,
        plane_projection, jfa, jfa_distance, post[2], debug, depth_only, vegetation, downsample, upsample;
}


#endif
