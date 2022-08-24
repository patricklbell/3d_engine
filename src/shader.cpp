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
	bool unified_bloom = false;

    GLuint null_program;
    struct NullUniforms null_uniforms;

    GLuint unified_programs[2];
    struct UnifiedUniforms unified_uniforms[2];

	GLuint water_programs[2];
    struct WaterUniforms water_uniforms[2];

	GLuint gaussian_blur_program;
	struct GaussianBlurUniforms gaussian_blur_uniforms;

	GLuint distance_blur_program;
	struct DistanceBlurUniforms distance_blur_uniforms;

	GLuint debug_program;
	struct DebugUniforms debug_uniforms;

	GLuint post_program[2];
	struct PostUniforms post_uniforms[2];

	GLuint skybox_programs[2];
	struct SkyboxUniforms skybox_uniforms;

	GLuint depth_only_program;
	struct MvpUniforms depth_only_uniforms;

	GLuint white_program;
	struct WhiteUniforms white_uniforms;
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

	linked_shader_codes[shader_macro_i] = (char*)fragment_macro;	glShaderSource(fragment_shader_id, linked_shader_codes.size(), &linked_shader_codes[0], NULL);
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
void loadPostShader(std::string path){
	static const std::string macros[] = {"", "#define BLOOM 1\n"};
	for(int i = 0; i < 2; i++){
		// Create and compile our GLSL program from the shaders
		auto tmp = post_program[i];
		post_program[i] = loadShader(path, macros[i]);
		if(post_program[i] == GL_FALSE) {
			post_program[i] = tmp;
			return;
		}
		post_uniforms[i].resolution = glGetUniformLocation(post_program[i], "resolution");
		post_uniforms[i].projection = glGetUniformLocation(post_program[i], "projection");
		post_uniforms[i].view		= glGetUniformLocation(post_program[i], "view");

		glUseProgram(post_program[i]);
		// Set fixed locations for textures in GL_TEXTUREi
		glUniform1i(glGetUniformLocation(post_program[i], "pixel_map"), 0);
		glUniform1i(glGetUniformLocation(post_program[i], "bloom_map"), 1);
		glUniform1i(glGetUniformLocation(post_program[i], "depth_map"), 2);
		glUniform1i(glGetUniformLocation(post_program[i], "skybox"),    3);
	}
}
void loadDebugShader(std::string path){
	auto tmp = debug_program;
	// Create and compile our GLSL program from the shaders
	debug_program = loadShader(path);
	if(debug_program == GL_FALSE) {
		debug_program = tmp;
		return;
	}

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
	auto tmp = gaussian_blur_program;
	// Create and compile our GLSL program from the shaders
	gaussian_blur_program = loadShader(path);
	if(gaussian_blur_program == GL_FALSE) {
		gaussian_blur_program = tmp;
		return;
	}

	// Grab uniforms to modify
	gaussian_blur_uniforms.horizontal = glGetUniformLocation(gaussian_blur_program, "horizontal");

	glUseProgram(gaussian_blur_program);
	// Set fixed locations for textures in GL_TEXTUREi
	glUniform1i(glGetUniformLocation(gaussian_blur_program, "image"), 0);
}
void loadDistanceBlurShader(std::string path) {
	auto tmp = distance_blur_program;
	// Create and compile our GLSL program from the shaders
	distance_blur_program = loadShader(path);
	if (distance_blur_program == GL_FALSE) {
		distance_blur_program = tmp;
		return;
	}

	// Grab uniforms to modify
	distance_blur_uniforms.horizontal = glGetUniformLocation(distance_blur_program, "horizontal");

	glUseProgram(distance_blur_program);
	// Set fixed locations for textures in GL_TEXTUREi
	glUniform1i(glGetUniformLocation(distance_blur_program, "image"), 0);
}
void loadNullShader(std::string path){
	auto tmp = null_program;
	// Create and compile our GLSL program from the shaders
	null_program = loadShader(path, graphics::shadow_invocation_macro, true);
	if(null_program == GL_FALSE) {
		null_program = tmp;
		return;
	}

	// Grab uniforms to modify
	null_uniforms.model = glGetUniformLocation(null_program, "model");
	
	glUseProgram(null_program);
}
void loadUnifiedShader(std::string path){
    static const std::string macros[] = {
        std::string("") + graphics::shadow_macro,
        std::string("#define BLOOM 1\n") + graphics::shadow_macro
    };
	for(int i = 0; i < 2; i++){
		// Create and compile our GLSL program from the shaders
		auto tmp = unified_programs[i];
		unified_programs[i] = loadShader(path, macros[i]);
		if(unified_programs[i] == GL_FALSE) {
			unified_programs[i] = tmp;
			return;
		}
		// Grab uniforms to modify during rendering
		unified_uniforms[i].mvp                         = glGetUniformLocation(unified_programs[i], "mvp");
		unified_uniforms[i].model                       = glGetUniformLocation(unified_programs[i], "model");
		unified_uniforms[i].sun_color                   = glGetUniformLocation(unified_programs[i], "sun_color");
		unified_uniforms[i].sun_direction               = glGetUniformLocation(unified_programs[i], "sun_direction");
		unified_uniforms[i].camera_position             = glGetUniformLocation(unified_programs[i], "camera_position");
        unified_uniforms[i].shadow_cascade_distances    = glGetUniformLocation(unified_programs[i], "shadow_cascade_distances");
        unified_uniforms[i].far_plane                   = glGetUniformLocation(unified_programs[i], "far_plane");
        unified_uniforms[i].view                        = glGetUniformLocation(unified_programs[i], "view");
		
		glUseProgram(unified_programs[i]);
		// Set fixed locations for textures in GL_TEXTUREi
		glUniform1i(glGetUniformLocation(unified_programs[i], "albedo_map"),   0);
		glUniform1i(glGetUniformLocation(unified_programs[i], "normal_map"),   1);
		glUniform1i(glGetUniformLocation(unified_programs[i], "metallic_map"), 2);
		glUniform1i(glGetUniformLocation(unified_programs[i], "roughness_map"),3);
		glUniform1i(glGetUniformLocation(unified_programs[i], "ao_map"),       4);
		glUniform1i(glGetUniformLocation(unified_programs[i], "shadow_map"),   5);
	}
}
void loadWaterShader(std::string path){
    static const std::string macros[] = {
        std::string("") + graphics::shadow_macro,
        std::string("#define BLOOM 1\n") + graphics::shadow_macro
    };
	for(int i = 0; i < 2; i++){
		// Create and compile our GLSL program from the shaders
		auto tmp = water_programs[i];
		water_programs[i] = loadShader(path, macros[i]);
		if(water_programs[i] == GL_FALSE) {
			water_programs[i] = tmp;
			return;
		}
		// Grab uniforms to modify during rendering
		water_uniforms[i].mvp                       = glGetUniformLocation(water_programs[i], "mvp");
		water_uniforms[i].model                     = glGetUniformLocation(water_programs[i], "model");
		water_uniforms[i].sun_color                 = glGetUniformLocation(water_programs[i], "sun_color");
		water_uniforms[i].sun_direction             = glGetUniformLocation(water_programs[i], "sun_direction");
		water_uniforms[i].camera_position           = glGetUniformLocation(water_programs[i], "camera_position");
		water_uniforms[i].time                      = glGetUniformLocation(water_programs[i], "time");
		water_uniforms[i].shallow_color             = glGetUniformLocation(water_programs[i], "shallow_color");
		water_uniforms[i].deep_color                = glGetUniformLocation(water_programs[i], "deep_color");
		water_uniforms[i].foam_color                = glGetUniformLocation(water_programs[i], "foam_color");
		water_uniforms[i].resolution                = glGetUniformLocation(water_programs[i], "resolution");
        water_uniforms[i].shadow_cascade_distances  = glGetUniformLocation(water_programs[i], "shadow_cascade_distances");
        water_uniforms[i].far_plane                 = glGetUniformLocation(water_programs[i], "far_plane");
        water_uniforms[i].view                      = glGetUniformLocation(water_programs[i], "view");
		
		glUseProgram(water_programs[i]);
		// Set fixed locations for textures in GL_TEXTUREi
		glUniform1i(glGetUniformLocation(water_programs[i], "screen_map"),       0);
		glUniform1i(glGetUniformLocation(water_programs[i], "depth_map"),        1);
		glUniform1i(glGetUniformLocation(water_programs[i], "simplex_gradient"), 2);
		glUniform1i(glGetUniformLocation(water_programs[i], "simplex_value"),    3);
		glUniform1i(glGetUniformLocation(water_programs[i], "collider"),		 4);
		glUniform1i(glGetUniformLocation(water_programs[i], "shadow_map"),       5);
	}
}
void loadSkyboxShader(std::string path){
	static const std::string macros[] = {
		std::string(""),
		std::string("#define BLOOM 1\n")
	};
	for (int i = 0; i < 2; i++) {
		// Create and compile our GLSL program from the shaders
		auto tmp = skybox_programs[i];
		skybox_programs[i] = loadShader(path, macros[i]);
		if (skybox_programs[i] == GL_FALSE) {
			skybox_programs[i] = tmp;
			return;
		}

		// Grab uniforms to modify during rendering
		skybox_uniforms.projection = glGetUniformLocation(skybox_programs[i], "projection");
		skybox_uniforms.view	   = glGetUniformLocation(skybox_programs[i], "view");
	}
}
void loadWhiteShader(std::string path){
	// Create and compile our GLSL program from the shaders
	auto tmp = white_program;
	white_program = loadShader(path, "", true);
	if (white_program == GL_FALSE) {
		white_program = tmp;
		return;
	}

	// Grab uniforms to modify during rendering
	white_uniforms.mvp    = glGetUniformLocation(white_program, "mvp");
	white_uniforms.height = glGetUniformLocation(white_program, "height");
}
void loadDepthOnlyShader(std::string path){
	// Create and compile our GLSL program from the shaders
	auto tmp = depth_only_program;
	depth_only_program = loadShader(path);
	if (depth_only_program == GL_FALSE) {
		depth_only_program = tmp;
		return;
	}

	// Grab uniforms to modify during rendering
	depth_only_uniforms.mvp = glGetUniformLocation(depth_only_program, "mvp");
}
void deleteShaderPrograms(){
    glDeleteProgram(null_program);
    glDeleteProgram(debug_program);
    glDeleteProgram(white_program);
    glDeleteProgram(depth_only_program);
    glDeleteProgram(unified_programs[0]);
    glDeleteProgram(unified_programs[1]);
    glDeleteProgram(gaussian_blur_program);
	glDeleteProgram(distance_blur_program);
	glDeleteProgram(post_program[0]);
	glDeleteProgram(post_program[1]);
	glDeleteProgram(skybox_programs[0]);
	glDeleteProgram(skybox_programs[1]);
}

void loadShader(std::string path, TYPE type) {
    switch (type) {
        case shader::TYPE::NULL_SHADER:
            loadNullShader(path);
            break;
        case shader::TYPE::UNIFIED_SHADER:
            loadUnifiedShader(path);
            break;
        case shader::TYPE::WATER_SHADER:
            loadWaterShader(path);
            break;
        case shader::TYPE::GAUSSIAN_BLUR_SHADER:
            loadGaussianBlurShader(path);
            break;
		case shader::TYPE::DISTANCE_BLUR_SHADER:
			loadDistanceBlurShader(path);
			break;
        case shader::TYPE::POST_SHADER:
            loadPostShader(path);
            break;
        case shader::TYPE::DEBUG_SHADER:
            loadDebugShader(path);
            break;
        case shader::TYPE::SKYBOX_SHADER:
            loadSkyboxShader(path);
            break;
        case shader::TYPE::WHITE_SHADER:
            loadWhiteShader(path);
            break;
        case shader::TYPE::DEPTH_ONLY_SHADER:
            loadDepthOnlyShader(path);
            break;
    }
}
