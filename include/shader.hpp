#ifndef SHADER_HPP
#define SHADER_HPP

#include <string>

#include <GLFW/glfw3.h>

enum class ShaderType : uint16_t {
    _NULL = 0,
    ANIMATED_NULL,
    UNIFIED,
    ANIMATED_UNIFIED,
    WATER,
    GAUSSIAN_BLUR,
    PLANE_PROJECTION,
    JFA,
    JFA_DISTANCE,
    POST,
    DEBUG,
    SKYBOX,
    DEPTH_ONLY,
    VEGETATION,
    DOWNSAMPLE,
    UPSAMPLE,
    NUM_TYPES,
};


GLuint loadShader(std::string vertex_fragment_file_path, std::string macro_prepends, bool geometry);
void loadNullShader(std::string path);
void loadAnimatedNullShader(std::string path);
void loadUnifiedShader(std::string path);
void loadAnimatedUnifiedShader(std::string path);
void loadWaterShader(std::string path);
void loadGaussianBlurShader(std::string path);
void loadPostShader(std::string path);
void loadDebugShader(std::string path);
void loadSkyboxShader(std::string path);
void loadVegetationShader(std::string path);
void loadDownsampleShader(std::string path);
void loadUpsampleShader(std::string path);
void loadShader(std::string path, ShaderType type);
void deleteShaderPrograms();

namespace shader {
	extern bool unified_bloom;

    extern GLuint null_program;
    extern struct NullUniforms {
        GLuint model; 
    } null_uniforms;

    extern GLuint animated_null_program;
    extern NullUniforms animated_null_uniforms;

    extern GLuint unified_program;
    extern struct UnifiedUniforms {
        GLuint mvp, model, sun_color, sun_direction, camera_position, shadow_cascade_distances, far_plane, view, albedo_mult, roughness_mult, ao_mult, metal_mult;
    } unified_uniforms;

    extern GLuint animated_unified_program;
    extern struct AnimatedUnifiedUniforms {
        GLuint mvp, model, sun_color, sun_direction, camera_position, shadow_cascade_distances, far_plane, view, albedo_mult, roughness_mult, ao_mult, metal_mult, bone_matrices;
    } animated_unified_uniforms;

    extern GLuint water_program;
    extern struct WaterUniforms {
        GLuint mvp, model, view, sun_color, sun_direction, camera_position, time, shallow_color, deep_color, foam_color, resolution, shadow_cascade_distances, far_plane;
    } water_uniforms;

    extern GLuint gaussian_blur_program;
    extern struct GaussianBlurUniforms {
        GLuint horizontal;
    } gaussian_blur_uniforms;

	extern GLuint plane_projection_program;
    extern struct WhiteUniforms {
        GLuint m;
    } plane_projection_uniforms;

    extern GLuint jfa_program;
    extern struct JfaUniforms {
        GLuint step, num_steps, resolution;
    } jfa_uniforms;

    extern GLuint jfa_distance_program;
    extern struct JfaDistanceUniforms {
        GLuint dimensions;
    } jfa_distance_uniforms;

    extern GLuint debug_program;
    extern struct DebugUniforms {
        GLuint mvp, model, sun_direction, time, flashing, shaded, color, color_flash_to;
    } debug_uniforms;

    extern GLuint post_programs[2];
    extern struct PostUniforms {
        GLuint resolution, projection, tan_half_fov, inverse_projection_untranslated_view;
    } post_uniforms[2];

    extern GLuint skybox_program;
    extern struct SkyboxUniforms {
        GLuint view, projection;
    } skybox_uniforms;

	extern GLuint depth_only_program;
	extern struct MvpUniforms {
		GLuint mvp;
	} depth_only_uniforms;

	extern GLuint vegetation_program;
	extern struct VegetationUniforms {
		GLuint mvp, time;
	} vegetation_uniforms;

	extern GLuint downsample_program;
	extern struct DownsampleUniforms {
		GLuint resolution, is_mip0;
	} downsample_uniforms;

	extern GLuint upsample_program;
}


#endif
