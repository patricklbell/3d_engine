#ifndef SHADER_HPP
#define SHADER_HPP

#include <string>

#include <GLFW/glfw3.h>

GLuint loadShader(std::string  vertex_fragment_file_path);
void loadNullShader(std::string path);
void loadUnifiedShader(std::string path);
void deleteShaderPrograms();

namespace shader {
    enum TYPE {
        NULL_SHADER = 0,
        UNIFIED_SHADER,
        NUM_SHADER_TYPES,
    };
    extern GLuint null_program;
    extern struct NullUniforms {
        GLuint mvp; 
    } null_uniforms;

    extern GLuint unified_program;
    extern struct UnifiedUniforms {
        GLuint mvp, model, sun_color, sun_direction, specular_exp, specular_int, camera_position;
    } unified_uniforms;
}

#endif
