#ifndef SHADER_HPP
#define SHADER_HPP

#include <string>

#include <GLFW/glfw3.h>

GLuint loadShader(std::string vertex_fragment_file_path, std::string macro_prepends);
void loadNullShader(std::string path);
void loadUnifiedShader(std::string path);
void loadGaussianBlurShader(std::string path);
void loadPostShader(std::string path);
void loadDebugShader(std::string path);
void deleteShaderPrograms();

namespace shader {
    enum TYPE {
        NULL_SHADER = 0,
        UNIFIED_SHADER,
        GAUSSIAN_BLUR_SHADER,
        POST_SHADER,
        DEBUG_SHADER,
        NUM_SHADER_TYPES,
    };
    extern GLuint null_program;
    extern struct NullUniforms {
        GLuint mvp; 
    } null_uniforms;

    extern GLuint unified_programs[2];
    extern struct UnifiedUniforms {
        GLuint mvp, shadow_mvp, model, sun_color, sun_direction, specular_exp, specular_int, camera_position;
    } unified_uniforms[2];
	extern bool unified_bloom;

    extern GLuint gaussian_blur_program;
    extern struct GaussianBlurUniforms {
        GLuint horizontal;
    } gaussian_blur_uniforms;

    extern GLuint debug_program;
    extern struct DebugUniforms {
        GLuint mvp, model, sun_direction, time, flashing, shaded, color, color_flash_to;
    } debug_uniforms;

    extern GLuint post_program;
}

#endif
