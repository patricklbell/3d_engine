#include <cstring>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <iostream>
#include <unordered_set>

// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <GLFW/glfw3.h>
#ifdef _WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <ShellScalingApi.h>
#endif

#include "shader.hpp"
#include "utilities.hpp"
#include "graphics.hpp"
#include "globals.hpp"

namespace shader {
	bool unified_bloom = true;

    GLuint null_program;
    struct NullUniforms null_uniforms;

	GLuint animated_null_program;
	struct NullUniforms animated_null_uniforms;

    GLuint unified_program;
    struct UnifiedUniforms unified_uniforms;

	GLuint animated_unified_program;
	struct AnimatedUnifiedUniforms animated_unified_uniforms;

	GLuint water_program;
    struct WaterUniforms water_uniforms;

	GLuint gaussian_blur_program;
	struct GaussianBlurUniforms gaussian_blur_uniforms;

	GLuint plane_projection_program;
	struct WhiteUniforms plane_projection_uniforms;

	GLuint jfa_program;
	struct JfaUniforms jfa_uniforms;

	GLuint jfa_distance_program;
	struct JfaDistanceUniforms jfa_distance_uniforms;

	GLuint debug_program;
	struct DebugUniforms debug_uniforms;

	GLuint post_programs[2];
	struct PostUniforms post_uniforms[2];

	GLuint skybox_program;
	struct SkyboxUniforms skybox_uniforms;

	GLuint depth_only_program;
	struct MvpUniforms depth_only_uniforms;

	GLuint vegetation_program;
	struct VegetationUniforms vegetation_uniforms;

	GLuint downsample_program;
	struct DownsampleUniforms downsample_uniforms;

	GLuint upsample_program;
}

using namespace shader;

static std::string read_line(char* data, int data_len, int &line_len) {
	for (int offset = 0; offset < data_len; offset++) {
		if (data[offset] == '\n') {
			line_len = offset + 1;
			std::string line;
			line.resize(offset);
			memcpy(&line[0], data, offset);
			return line;
		} else if (data[offset] == '\r') {
			line_len = offset + 2;
			std::string line;
			line.resize(offset);
			memcpy(&line[0], data, offset);
			return line;
		}
	}
	return "";
}

static char *read_file_contents(std::string path, int &num_bytes) {
	FILE *fp = fopen(path.c_str(), "rb");

	if (fp == NULL) {
		return nullptr;
	}
	fseek(fp, 0L, SEEK_END);
	num_bytes = ftell(fp);
	
	// @note adds \0 to fread
	rewind(fp);
	char *data = (char*)malloc((num_bytes + 1) * sizeof(char));
	if (data == NULL)
		return nullptr;
	fread(data, sizeof(char), num_bytes, fp);
	fclose(fp);

	data[num_bytes] = '\0';

	return data;
}

static inline void free_linked_shader_codes(std::vector<char *> &lsc, int beg, int end) {
	for(int i = beg; i < end; ++i) {
		free(lsc[i]);
	}
}

static void load_shader_dependencies(char *shader_code, int num_bytes, 
		std::vector<char *> &linked_shader_codes, std::unordered_set<std::string> &linked_shader_paths) {
	int offset = 0;

	constexpr std::string_view load_macro = "#load ";
	for(int line_i = 0; offset < num_bytes; ++line_i) {
		int aoffset;
		auto line = read_line(&shader_code[offset], num_bytes, aoffset);

		if(startsWith(line, load_macro)){
			memset(&shader_code[offset], ' ', aoffset);
			if(line.size() > load_macro.size()) {
				// @todo make this handle any relative path
				std::string loadpath = "data/shaders/" + line.substr(load_macro.size());

				int loaded_num_bytes;
				if(linked_shader_paths.find(loadpath) == linked_shader_paths.end()) {
					linked_shader_paths.insert(loadpath);

					char * loaded_shader = read_file_contents(loadpath, loaded_num_bytes);
					if(loaded_shader != nullptr) {
						std::cout << "Successful #load at path " << loadpath << "\n";
						load_shader_dependencies(loaded_shader, loaded_num_bytes, linked_shader_codes, linked_shader_paths);
						linked_shader_codes.push_back(loaded_shader);	
					} else {
						std::cout << "Failed to #load path " << loadpath << "\n";
					}
				} else {
					std::cout << "Skipped duplicate #load of path " << loadpath << "\n";
				}
			}
		}
		offset += aoffset;
	}
}

GLuint loadShader(std::string vertex_fragment_file_path, std::string macro_prepends="", bool geometry=false) {
	const char *path = vertex_fragment_file_path.c_str();
	std::cout << "Loading shader " << path << ".\n";
	const char *fragment_macro = "#define COMPILING_FS 1\n";	const char *vertex_macro   = "#define COMPILING_VS 1\n";
	
	GLuint vertex_shader_id   = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);

	int num_bytes;
	char *shader_code = read_file_contents(vertex_fragment_file_path, num_bytes);
	if(shader_code == nullptr) {
		std::cerr << "Can't open shader file " << path << ".\n";
		return GL_FALSE;
	}

	std::vector<char *> linked_shader_codes = {(char*)glsl_version.c_str(), (char*)macro_prepends.c_str()};
	std::unordered_set<std::string> linked_shader_paths;
	int loaded_shader_beg_i = linked_shader_codes.size();

	load_shader_dependencies(shader_code, num_bytes, linked_shader_codes, linked_shader_paths);

	int shader_macro_i = linked_shader_codes.size();
	linked_shader_codes.push_back((char*)vertex_macro);
	linked_shader_codes.push_back((char*)shader_code);
	glShaderSource(vertex_shader_id, linked_shader_codes.size(), &linked_shader_codes[0], NULL);
	glCompileShader(vertex_shader_id);

	int info_log_length;
	glGetShaderiv(vertex_shader_id, GL_INFO_LOG_LENGTH, &info_log_length);
	if ( info_log_length > 0 ){
		char *vertex_shader_error_message = (char *)malloc(sizeof(char) * (info_log_length+1));
		glGetShaderInfoLog(vertex_shader_id, info_log_length, NULL, vertex_shader_error_message);
		std::cerr << "Vertex shader:\n" << vertex_shader_error_message << "\n";
		free(vertex_shader_error_message);
		free_linked_shader_codes(linked_shader_codes, loaded_shader_beg_i, shader_macro_i);
    	free(shader_code);
		return GL_FALSE;
	}

	linked_shader_codes[shader_macro_i] = (char*)fragment_macro;	
	glShaderSource(fragment_shader_id, linked_shader_codes.size(), &linked_shader_codes[0], NULL);
	glCompileShader(fragment_shader_id);

	glGetShaderiv(fragment_shader_id, GL_INFO_LOG_LENGTH, &info_log_length);
	if ( info_log_length > 0 ){
		char *fragment_shader_error_message = (char *)malloc(sizeof(char) * (info_log_length+1));
		glGetShaderInfoLog(fragment_shader_id, info_log_length, NULL, fragment_shader_error_message);
		std::cerr << "Fragment shader:\n" << fragment_shader_error_message << "\n";
		free(fragment_shader_error_message);
		free_linked_shader_codes(linked_shader_codes, loaded_shader_beg_i, shader_macro_i);
    	free(shader_code);
		return GL_FALSE;
	}

	GLuint geometry_shader_id;
	if(geometry){
		std::cout << "Compiling additional geometry shader.\n";
		const char *geometry_macro = "#define COMPILING_GS 1\n";

		geometry_shader_id = glCreateShader(GL_GEOMETRY_SHADER);
		linked_shader_codes[shader_macro_i] = (char*)geometry_macro;
		glShaderSource(geometry_shader_id, linked_shader_codes.size(), &linked_shader_codes[0], NULL);
		glCompileShader(geometry_shader_id);

		glGetShaderiv(geometry_shader_id, GL_INFO_LOG_LENGTH, &info_log_length);
		if ( info_log_length > 0 ){
			char *geometry_shader_error_message = (char *)malloc(sizeof(char) * (info_log_length+1));
			glGetShaderInfoLog(geometry_shader_id, info_log_length, NULL, geometry_shader_error_message);
			std::cerr << "Geometry shader:\n" << geometry_shader_error_message << "\n";
			free(geometry_shader_error_message);
			free_linked_shader_codes(linked_shader_codes, loaded_shader_beg_i, shader_macro_i);
			free(shader_code);
			return GL_FALSE;
		}
	}

	GLuint program_id = glCreateProgram();
	glAttachShader(program_id, vertex_shader_id);
	if(geometry) glAttachShader(program_id, geometry_shader_id);
	glAttachShader(program_id, fragment_shader_id);
	glLinkProgram(program_id);

	glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &info_log_length);
	if ( info_log_length > 0 ){
		char *program_error_message = (char *)malloc(sizeof(char) * (info_log_length+1));
		glGetProgramInfoLog(program_id, info_log_length, NULL, program_error_message);
		std::cerr << "Program attaching:\n" << program_error_message << "\n";
		free(program_error_message);
		free_linked_shader_codes(linked_shader_codes, loaded_shader_beg_i, shader_macro_i);
    	free(shader_code);
		return GL_FALSE;
	}

	glDetachShader(program_id, vertex_shader_id);
	if(geometry) glDetachShader(program_id, geometry_shader_id);
	glDetachShader(program_id, fragment_shader_id);
	
	glDeleteShader(vertex_shader_id);
	if(geometry) glDeleteShader(geometry_shader_id);
	glDeleteShader(fragment_shader_id);

	free_linked_shader_codes(linked_shader_codes, loaded_shader_beg_i, shader_macro_i);
    free(shader_code);

	return program_id;
}

template<typename... Args>
static bool attemptLoadShader(GLuint &dest, const std::string &path, Args&&... args) {
	auto tmp = dest;
	dest = loadShader(path, std::forward<Args>(args)...);
	if(dest == GL_FALSE) {
		dest = tmp;
		return false;
	}
	return true;
}

void loadPostShader(std::string path){
	static const std::string macros[] = { "", "#define BLOOM 1\n"};

	for(int i = 0; i < 2; ++i) {
		if(!attemptLoadShader(post_programs[i], path, macros[i])) 
			return;

		post_uniforms[i].inverse_projection_untranslated_view = glGetUniformLocation(post_programs[i], "inverse_projection_untranslated_view");
		post_uniforms[i].resolution			= glGetUniformLocation(post_programs[i], "resolution");
		post_uniforms[i].projection			= glGetUniformLocation(post_programs[i], "projection");
		post_uniforms[i].tan_half_fov		= glGetUniformLocation(post_programs[i], "tan_half_fov");

		glUseProgram(post_programs[i]);
		// Set fixed locations for textures in GL_TEXTUREi
		glUniform1i(glGetUniformLocation(post_programs[i], "pixel_map"), 0);
		glUniform1i(glGetUniformLocation(post_programs[i], "bloom_map"), 1);
		glUniform1i(glGetUniformLocation(post_programs[i], "depth_map"), 2);
		glUniform1i(glGetUniformLocation(post_programs[i], "skybox"),    3);
	}
}
void loadDebugShader(std::string path){
	if(!attemptLoadShader(debug_program, path))
		return;

	// Grab uniforms to modify
	debug_uniforms.model			= glGetUniformLocation(debug_program, "model");
	debug_uniforms.mvp				= glGetUniformLocation(debug_program, "mvp");
	debug_uniforms.sun_direction	= glGetUniformLocation(debug_program, "sun_direction");
	debug_uniforms.time				= glGetUniformLocation(debug_program, "time");
	debug_uniforms.flashing			= glGetUniformLocation(debug_program, "flashing");
	debug_uniforms.shaded			= glGetUniformLocation(debug_program, "shaded");
	debug_uniforms.color			= glGetUniformLocation(debug_program, "color");
	debug_uniforms.color_flash_to	= glGetUniformLocation(debug_program, "color_flash_to");
}
void loadGaussianBlurShader(std::string path){
	if(!attemptLoadShader(gaussian_blur_program, path))
		return;

	// Grab uniforms to modify
	gaussian_blur_uniforms.horizontal = glGetUniformLocation(gaussian_blur_program, "horizontal");

	glUseProgram(gaussian_blur_program);
	// Set fixed locations for textures in GL_TEXTUREi
	glUniform1i(glGetUniformLocation(gaussian_blur_program, "image"), 0);
}

void loadPlaneProjectionShader(std::string path) {
	if(!attemptLoadShader(plane_projection_program, path, "", true))
		return;

	// Grab uniforms to modify during rendering
	plane_projection_uniforms.m = glGetUniformLocation(plane_projection_program, "m");
}

void loadJfaShader(std::string path) {
	if(!attemptLoadShader(jfa_program, path))
		return;

	// Grab uniforms to modify
	jfa_uniforms.step	    = glGetUniformLocation(jfa_program, "step");
	jfa_uniforms.num_steps	= glGetUniformLocation(jfa_program, "num_steps");
	jfa_uniforms.resolution	= glGetUniformLocation(jfa_program, "resolution");

	glUseProgram(jfa_program);
	// Set fixed locations for textures in GL_TEXTUREi
	glUniform1i(glGetUniformLocation(jfa_program, "image"), 0);
}

void loadJfaDistanceShader(std::string path) {
	if(!attemptLoadShader(jfa_distance_program, path))
		return;

	jfa_distance_uniforms.dimensions = glGetUniformLocation(jfa_distance_program, "dimensions");

	glUseProgram(jfa_distance_program);
	// Set fixed locations for textures in GL_TEXTUREi
	glUniform1i(glGetUniformLocation(jfa_distance_program, "image"), 0);
}

void loadNullShader(std::string path){
	if(!attemptLoadShader(null_program, path, graphics::shadow_invocation_macro, true))
		return;

	// Grab uniforms to modify
	null_uniforms.model = glGetUniformLocation(null_program, "model");
}

void loadAnimatedNullShader(std::string path) {
	if(!attemptLoadShader(animated_null_program, path, graphics::shadow_invocation_macro, true))
		return;

	// Grab uniforms to modify
	animated_null_uniforms.model = glGetUniformLocation(animated_null_program, "model");
}

void loadUnifiedShader(std::string path){
	if(!attemptLoadShader(unified_program, path, graphics::shadow_macro))
		return;

	// Grab uniforms to modify during rendering
	unified_uniforms.mvp                         = glGetUniformLocation(unified_program, "mvp");
	unified_uniforms.model                       = glGetUniformLocation(unified_program, "model");
	unified_uniforms.sun_color                   = glGetUniformLocation(unified_program, "sun_color");
	unified_uniforms.sun_direction               = glGetUniformLocation(unified_program, "sun_direction");
	unified_uniforms.camera_position             = glGetUniformLocation(unified_program, "camera_position");
    unified_uniforms.shadow_cascade_distances    = glGetUniformLocation(unified_program, "shadow_cascade_distances");
    unified_uniforms.far_plane                   = glGetUniformLocation(unified_program, "far_plane");
    unified_uniforms.view                        = glGetUniformLocation(unified_program, "view");
	unified_uniforms.albedo_mult					= glGetUniformLocation(unified_program, "albedo_mult");
	unified_uniforms.roughness_mult				= glGetUniformLocation(unified_program, "roughness_mult");
	unified_uniforms.ao_mult						= glGetUniformLocation(unified_program, "ao_mult");
	unified_uniforms.metal_mult					= glGetUniformLocation(unified_program, "metal_mult");
	
	glUseProgram(unified_program);
	// Set fixed locations for textures in GL_TEXTUREi
	glUniform1i(glGetUniformLocation(unified_program, "albedo_map"),   0);
	glUniform1i(glGetUniformLocation(unified_program, "normal_map"),   1);
	glUniform1i(glGetUniformLocation(unified_program, "metallic_map"), 2);
	glUniform1i(glGetUniformLocation(unified_program, "roughness_map"),3);
	glUniform1i(glGetUniformLocation(unified_program, "ao_map"),       4);
	glUniform1i(glGetUniformLocation(unified_program, "shadow_map"),   5);
}

void loadAnimatedUnifiedShader(std::string path) {
	if(!attemptLoadShader(animated_unified_program, path, graphics::shadow_macro))
		return;

	// Grab uniforms to modify during rendering
	animated_unified_uniforms.mvp = glGetUniformLocation(animated_unified_program, "mvp");
	animated_unified_uniforms.model = glGetUniformLocation(animated_unified_program, "model");
	animated_unified_uniforms.sun_color = glGetUniformLocation(animated_unified_program, "sun_color");
	animated_unified_uniforms.sun_direction = glGetUniformLocation(animated_unified_program, "sun_direction");
	animated_unified_uniforms.camera_position = glGetUniformLocation(animated_unified_program, "camera_position");
	animated_unified_uniforms.shadow_cascade_distances = glGetUniformLocation(animated_unified_program, "shadow_cascade_distances");
	animated_unified_uniforms.far_plane = glGetUniformLocation(animated_unified_program, "far_plane");
	animated_unified_uniforms.view = glGetUniformLocation(animated_unified_program, "view");
	animated_unified_uniforms.albedo_mult = glGetUniformLocation(animated_unified_program, "albedo_mult");
	animated_unified_uniforms.roughness_mult = glGetUniformLocation(animated_unified_program, "roughness_mult");
	animated_unified_uniforms.ao_mult = glGetUniformLocation(animated_unified_program, "ao_mult");
	animated_unified_uniforms.metal_mult = glGetUniformLocation(animated_unified_program, "metal_mult");

	glUseProgram(animated_unified_program);
	// Set fixed locations for textures in GL_TEXTUREi
	glUniform1i(glGetUniformLocation(animated_unified_program, "albedo_map"), 0);
	glUniform1i(glGetUniformLocation(animated_unified_program, "normal_map"), 1);
	glUniform1i(glGetUniformLocation(animated_unified_program, "metallic_map"), 2);
	glUniform1i(glGetUniformLocation(animated_unified_program, "roughness_map"), 3);
	glUniform1i(glGetUniformLocation(animated_unified_program, "ao_map"), 4);
	glUniform1i(glGetUniformLocation(animated_unified_program, "shadow_map"), 5);
}

void loadWaterShader(std::string path){
	if(!attemptLoadShader(water_program, path, graphics::shadow_macro))
		return;

	// Grab uniforms to modify during rendering
	water_uniforms.mvp                       = glGetUniformLocation(water_program, "mvp");
	water_uniforms.model                     = glGetUniformLocation(water_program, "model");
	water_uniforms.sun_color                 = glGetUniformLocation(water_program, "sun_color");
	water_uniforms.sun_direction             = glGetUniformLocation(water_program, "sun_direction");
	water_uniforms.camera_position           = glGetUniformLocation(water_program, "camera_position");
	water_uniforms.time                      = glGetUniformLocation(water_program, "time");
	water_uniforms.shallow_color             = glGetUniformLocation(water_program, "shallow_color");
	water_uniforms.deep_color                = glGetUniformLocation(water_program, "deep_color");
	water_uniforms.foam_color                = glGetUniformLocation(water_program, "foam_color");
	water_uniforms.resolution                = glGetUniformLocation(water_program, "resolution");
    water_uniforms.shadow_cascade_distances  = glGetUniformLocation(water_program, "shadow_cascade_distances");
    water_uniforms.far_plane                 = glGetUniformLocation(water_program, "far_plane");
    water_uniforms.view                      = glGetUniformLocation(water_program, "view");
	
	glUseProgram(water_program);
	// Set fixed locations for textures in GL_TEXTUREi
	glUniform1i(glGetUniformLocation(water_program, "screen_map"),       0);
	glUniform1i(glGetUniformLocation(water_program, "depth_map"),        1);
	glUniform1i(glGetUniformLocation(water_program, "simplex_gradient"), 2);
	glUniform1i(glGetUniformLocation(water_program, "simplex_value"),    3);
	glUniform1i(glGetUniformLocation(water_program, "collider"),		 4);
	glUniform1i(glGetUniformLocation(water_program, "shadow_map"),       5);
}
void loadSkyboxShader(std::string path){
	if(!attemptLoadShader(skybox_program, path))
		return;

	// Grab uniforms to modify during rendering
	skybox_uniforms.projection = glGetUniformLocation(skybox_program, "projection");
	skybox_uniforms.view	   = glGetUniformLocation(skybox_program, "view");
}
void loadDepthOnlyShader(std::string path){
	if(!attemptLoadShader(depth_only_program, path))
		return;

	// Grab uniforms to modify during rendering
	depth_only_uniforms.mvp = glGetUniformLocation(depth_only_program, "mvp");
}
void loadVegetationShader(std::string path){
	if(!attemptLoadShader(vegetation_program, path))
		return;

	// Grab uniforms to modify during rendering
	vegetation_uniforms.mvp  = glGetUniformLocation(vegetation_program, "mvp");
	vegetation_uniforms.time = glGetUniformLocation(vegetation_program, "time");

	glUseProgram(vegetation_program);
	// Set fixed locations for textures in GL_TEXTUREi
	glUniform1i(glGetUniformLocation(vegetation_program, "image"), 0);
}
void loadDownsampleShader(std::string path){
	if(!attemptLoadShader(downsample_program, path))
		return;

	// Grab uniforms to modify during rendering
	downsample_uniforms.resolution = glGetUniformLocation(downsample_program, "resolution");
	downsample_uniforms.is_mip0    = glGetUniformLocation(downsample_program, "is_mip0");

	glUseProgram(downsample_program);
	// Set fixed locations for textures in GL_TEXTUREi
	glUniform1i(glGetUniformLocation(downsample_program, "image"), 0);
}
void loadUpsampleShader(std::string path){
	if(!attemptLoadShader(upsample_program, path))
		return;

	glUseProgram(upsample_program);
	// Set fixed locations for textures in GL_TEXTUREi
	glUniform1i(glGetUniformLocation(upsample_program, "image"), 0);
}

void deleteShaderPrograms(){
    glDeleteProgram(null_program);
    glDeleteProgram(debug_program);
    glDeleteProgram(depth_only_program);
    glDeleteProgram(unified_program);
    glDeleteProgram(gaussian_blur_program);
    glDeleteProgram(plane_projection_program);
	glDeleteProgram(jfa_program);
	glDeleteProgram(jfa_distance_program);
	glDeleteProgram(post_programs[0]);
	glDeleteProgram(post_programs[1]);
	glDeleteProgram(skybox_program);
	glDeleteProgram(vegetation_program);
	glDeleteProgram(downsample_program);
	glDeleteProgram(upsample_program);
}

void loadShader(std::string path, ShaderType type) {
    switch (type) {
        case ShaderType::_NULL:
            loadNullShader(path);
            break;
		case ShaderType::ANIMATED_NULL:
			loadAnimatedNullShader(path);
			break;
        case ShaderType::UNIFIED:
            loadUnifiedShader(path);
            break;
		case ShaderType::ANIMATED_UNIFIED:
			loadAnimatedUnifiedShader(path);
			break;
        case ShaderType::WATER:
            loadWaterShader(path);
            break;
        case ShaderType::GAUSSIAN_BLUR:
            loadGaussianBlurShader(path);
            break;
		case ShaderType::PLANE_PROJECTION:
			loadPlaneProjectionShader(path);
			break;
		case ShaderType::JFA:
			loadJfaShader(path);
			break;
		case ShaderType::JFA_DISTANCE:
			loadJfaDistanceShader(path);
			break;
        case ShaderType::POST:
            loadPostShader(path);
            break;
        case ShaderType::DEBUG:
            loadDebugShader(path);
            break;
        case ShaderType::SKYBOX:
            loadSkyboxShader(path);
            break;
        case ShaderType::DEPTH_ONLY:
            loadDepthOnlyShader(path);
            break;
        case ShaderType::VEGETATION:
            loadVegetationShader(path);
            break;
        case ShaderType::DOWNSAMPLE:
            loadDownsampleShader(path);
            break;
        case ShaderType::UPSAMPLE:
            loadUpsampleShader(path);
            break;
        default:
        	break;
    }
}
