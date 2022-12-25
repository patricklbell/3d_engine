#include <iostream>

#include <glm/glm.hpp>

#include <shader/globals.hpp>
#include "graphics.hpp"

namespace Shaders {
	Shader unified;
	Shader shadow;
	Shader water;
	Shader debug;
	Shader post;
	Shader skybox;

	Shader specular_convolution;
	Shader diffuse_convolution;
	Shader generate_brdf_lut;

	Shader gaussian_blur;
	Shader downsample;
	Shader blur_upsample;

	Shader plane_projection;
	Shader jump_flood;
	Shader jfa_to_distance;

	Shader volumetric_integration;
	Shader volumetric_raymarch;

	Shader lightmap_hemisphere;
	Shader lightmap_downsample;

	[[nodiscard]] bool init() {
		bool success = true;
		std::string prepend = gl_state.glsl_version + graphics::shadow_shader_macro + graphics::animation_macro + graphics::volumetric_shader_macro + graphics::lights_macro + '\0';

		success&=unified.load_file_compile("data/shaders/unified.gl", prepend);
		success&=shadow.load_file_compile("data/shaders/null.gl", prepend);
		success&=water.load_file_compile("data/shaders/water.gl", prepend);
		success&=debug.load_file_compile("data/shaders/debug.gl", prepend);
		success&=post.load_file_compile("data/shaders/post.gl", prepend);
		success&=skybox.load_file_compile("data/shaders/skybox.gl", prepend);

		success&=specular_convolution.load_file_compile("data/shaders/specular_convolution.gl", prepend);
		success&=diffuse_convolution.load_file_compile("data/shaders/diffuse_convolution.gl", prepend);
		success&=generate_brdf_lut.load_file_compile("data/shaders/generate_brdf_lut.gl", prepend);

		success&=gaussian_blur.load_file_compile("data/shaders/gaussian_blur.gl", prepend);
		success&=downsample.load_file_compile("data/shaders/downsample.gl", prepend);
		success&=blur_upsample.load_file_compile("data/shaders/blur_upsample.gl", prepend);

		success&=plane_projection.load_file_compile("data/shaders/plane_projection.gl", prepend);
		success&=jump_flood.load_file_compile("data/shaders/jump_flood.gl", prepend);
		success&=jfa_to_distance.load_file_compile("data/shaders/jfa_to_distance.gl", prepend);

		success&=volumetric_integration.load_file_compile("data/shaders/volumetric_integration.gl", prepend);
		success&=volumetric_raymarch.load_file_compile("data/shaders/volumetric_raymarch.gl", prepend);

		success &= lightmap_hemisphere.load_file_compile("data/shaders/lightmapper/hemisphere.gl", prepend);
		success &= lightmap_downsample.load_file_compile("data/shaders/lightmapper/downsample.gl", prepend);

		return success;
	}
	[[nodiscard]] bool live_update() {
		bool success = true;

		success&=unified.update_from_dependencies();
		success&=shadow.update_from_dependencies();
		success&=water.update_from_dependencies();
		success&=debug.update_from_dependencies();
		success&=post.update_from_dependencies();
		success&=skybox.update_from_dependencies();

		success&=specular_convolution.update_from_dependencies();
		success&=diffuse_convolution.update_from_dependencies();
		success&=generate_brdf_lut.update_from_dependencies();

		success&=gaussian_blur.update_from_dependencies();
		success&=downsample.update_from_dependencies();

		success&=plane_projection.update_from_dependencies();
		success&=jump_flood.update_from_dependencies();
		success&=jfa_to_distance.update_from_dependencies();

		success&=volumetric_integration.update_from_dependencies();
		success&=volumetric_raymarch.update_from_dependencies();

		success &= lightmap_hemisphere.update_from_dependencies();
		success &= lightmap_downsample.update_from_dependencies();

		return success;
	}
};