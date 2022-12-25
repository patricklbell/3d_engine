#ifndef SHADER_GLOBALS_HPP
#define SHADER_GLOBALS_HPP

#include <shader/core.hpp>

namespace Shaders {
	extern Shader unified;
	extern Shader shadow;
	extern Shader water;
	extern Shader debug;
	extern Shader post;
	extern Shader skybox;

	extern Shader specular_convolution;
	extern Shader diffuse_convolution;
	extern Shader generate_brdf_lut;

	extern Shader gaussian_blur;
	extern Shader downsample;
	extern Shader blur_upsample;

	extern Shader plane_projection;
	extern Shader jump_flood;
	extern Shader jfa_to_distance;

	extern Shader volumetric_integration;
	extern Shader volumetric_raymarch;

	extern Shader lightmap_hemisphere;
	extern Shader lightmap_downsample;

	bool init();
	bool live_update();
};

#endif SHADER_GLOBALS_HPP