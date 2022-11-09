#ifndef SHADER_HPP
#define SHADER_HPP

#include <string>
#include <unordered_map>

#include <GLFW/glfw3.h>

struct Shader {
    std::string handle = "";
    std::unordered_map<std::string, GLuint> uniforms;
    GLuint program = GL_FALSE;
    GLuint uniform(const std::string &name);
    ~Shader();
};

bool loadShader(Shader& shader, std::string_view vertex_fragment_file_path, std::string_view macros, const bool geometry, const bool compute);
void initGlobalShaders();
void updateGlobalShaders();

namespace shader {
    extern Shader animated_null, null, null_vegetation, unified, unified_ns, animated_unified, animated_unified_ns, water, water_ns, gaussian_blur,
        plane_projection, jfa, jfa_distance, post[2], debug, depth_only, vegetation, vegetation_ns, null_clip,
        downsample, upsample, diffuse_convolution, specular_convolution, generate_brdf_lut, skybox, volumetric_integration, volumetric_raymarch;
}


#endif
