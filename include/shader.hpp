#ifndef SHADER_HPP
#define SHADER_HPP

#include <string>

#include <GLFW/glfw3.h>

GLuint loadShader(std::string vertex_fragment_file_path, std::string macro_prepends, bool geometry);
void loadNullShader(std::string path);
void loadUnifiedShader(std::string path);
void loadWaterShader(std::string path);
void loadGaussianBlurShader(std::string path);
void loadPostShader(std::string path);
void loadDebugShader(std::string path);
void deleteShaderPrograms();

namespace shader {
	extern bool unified_bloom;

    enum TYPE {
        NULL_SHADER = 0,
        UNIFIED_SHADER,
        WATER_SHADER,
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
        GLuint mvp, shadow_mvp, model, sun_color, sun_direction, camera_position;
    } unified_uniforms[2];

    extern GLuint water_programs[2];
    extern struct WaterUniforms {
        GLuint mvp, shadow_mvp, model, sun_color, sun_direction, camera_position, time, shallow_color, deep_color, foam_color, resolution;
    } water_uniforms[2];

    extern GLuint gaussian_blur_program;
    extern struct GaussianBlurUniforms {
        GLuint horizontal;
    } gaussian_blur_uniforms;

    extern GLuint debug_program;
    extern struct DebugUniforms {
        GLuint mvp, model, sun_direction, time, flashing, shaded, color, color_flash_to;
    } debug_uniforms;

    extern GLuint post_program[2];
    extern struct PostUniforms {
        GLuint resolution;
    } post_uniforms[2];
}

#endif
