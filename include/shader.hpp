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
void loadSkyboxShader(std::string path);
void deleteShaderPrograms();

namespace shader {
	extern bool unified_bloom;

    enum class TYPE {
        NULL_SHADER = 0,
        UNIFIED_SHADER,
        WATER_SHADER,
        GAUSSIAN_BLUR_SHADER,
        DISTANCE_BLUR_SHADER,
        POST_SHADER,
        DEBUG_SHADER,
        SKYBOX_SHADER,
        WHITE_SHADER,
        DEPTH_ONLY_SHADER,
        NUM_SHADER_TYPES,
    };
    extern GLuint null_program;
    extern struct NullUniforms {
        GLuint model; 
    } null_uniforms;

    extern GLuint unified_programs[2];
    extern struct UnifiedUniforms {
        GLuint mvp, model, sun_color, sun_direction, camera_position, shadow_cascade_distances, far_plane, view, albedo_mult, roughness_mult, ao_mult, metal_mult;
    } unified_uniforms[2];

    extern GLuint water_programs[2];
    extern struct WaterUniforms {
        GLuint mvp, model, view, sun_color, sun_direction, camera_position, time, shallow_color, deep_color, foam_color, resolution, shadow_cascade_distances, far_plane;
    } water_uniforms[2];

    extern GLuint gaussian_blur_program;
    extern struct GaussianBlurUniforms {
        GLuint horizontal;
    } gaussian_blur_uniforms;

    extern GLuint distance_blur_program;
    extern struct DistanceBlurUniforms {
        GLuint horizontal;
    } distance_blur_uniforms;

    extern GLuint debug_program;
    extern struct DebugUniforms {
        GLuint mvp, model, sun_direction, time, flashing, shaded, color, color_flash_to;
    } debug_uniforms;

    extern GLuint post_program[2];
    extern struct PostUniforms {
        GLuint resolution, view, projection;
    } post_uniforms[2];

    extern GLuint skybox_programs[2];
    extern struct SkyboxUniforms {
        GLuint view, projection;
    } skybox_uniforms;

	extern GLuint depth_only_program;
	extern struct MvpUniforms {
		GLuint mvp;
	} depth_only_uniforms;

	extern GLuint white_program;
    extern struct WhiteUniforms {
        GLuint mvp, height;
    } white_uniforms;
}

void loadShader(std::string path, shader::TYPE type);

#endif
