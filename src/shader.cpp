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

#include "shader.hpp"
#include "utilities.hpp"
#include "graphics.hpp"
#include "globals.hpp"

Shader::~Shader() {
	glDeleteProgram(program);
}

GLuint Shader::uniform(const std::string &name) {
	auto lu = uniforms.find(name);
	if (lu == uniforms.end()) {
		std::cerr << "Unknown uniform " << name << " for shader " << handle << "\n";
		return GL_FALSE;
	}
	else {
		return lu->second;
	}
}

namespace shader {
	Shader animated_null, null, unified, animated_unified, water, gaussian_blur,
		plane_projection, jfa, jfa_distance, post[2], debug, depth_only, vegetation, downsample, upsample;
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

static char *read_file_contents(std::string_view path, int &num_bytes) {
	FILE *fp = fopen(path.data(), "rb");

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

bool loadShader(Shader& shader, std::string_view vertex_fragment_file_path, std::string_view macros="", bool geometry = false) {
	const char *path = vertex_fragment_file_path.data();
	std::cout << "Loading shader " << path << ".\n";
	const char *fragment_macro = "#define COMPILING_FS 1\n";	const char *vertex_macro   = "#define COMPILING_VS 1\n";
	
	GLuint vertex_shader_id   = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);

	int num_bytes;
	char *shader_code = read_file_contents(vertex_fragment_file_path, num_bytes);
	if(shader_code == nullptr) {
		std::cerr << "Can't open shader file " << path << ".\n";
		return false;
	}

	std::vector<char *> linked_shader_codes = {(char*)glsl_version.c_str(), (char*)macros.data()};
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
		return false;
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
		return false;
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
			return false;
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
		return false;
	}

	glDetachShader(program_id, vertex_shader_id);
	if(geometry) glDetachShader(program_id, geometry_shader_id);
	glDetachShader(program_id, fragment_shader_id);
	
	glDeleteShader(vertex_shader_id);
	if(geometry) glDeleteShader(geometry_shader_id);
	glDeleteShader(fragment_shader_id);

	free_linked_shader_codes(linked_shader_codes, loaded_shader_beg_i, shader_macro_i);
    free(shader_code);

	shader.program = program_id;
	shader.handle = std::string(vertex_fragment_file_path);

	GLint count;
	glGetProgramiv(shader.program, GL_ACTIVE_UNIFORMS, &count);
	printf("Active Uniforms: %d\n", count);

	const GLsizei buf_size = 256; // maximum name length
	//glGetProgramiv(shader.program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &buf_size);
	GLchar name[buf_size]; // variable name in GLSL
	GLsizei length; // name length

	GLint size; // size of the variable
	GLenum type; // type of the variable (float, vec3 or mat4, etc)
	for (GLint i = 0; i < count; i++) {
		glGetActiveUniform(shader.program, (GLuint)i, buf_size, &length, &size, &type, name);
		shader.uniforms[name] = glGetUniformLocation(shader.program, name);

		printf("Uniform #%d Type: %u Name: %s\n", i, type, name);
	}

	printf("\n");
	
	return true;
}

#include <filesystem>

struct ShaderData {
	std::string path;
	Shader* shader;
	bool is_geometry;
	std::string macro;
	std::filesystem::file_time_type update_time;
};

// Pretty clunky way of doing live update but workable for now
static std::vector<ShaderData> shader_list;
void initGlobalShaders() {
	std::filesystem::file_time_type empty_file_time;
	auto s = &animated_null;
	shader_list = {
		{"data/shaders/null_anim.gl",			&animated_null,		 true, graphics::shadow_shader_macro,		empty_file_time},
		{"data/shaders/null.gl",				&null,				 true, graphics::shadow_shader_macro,		empty_file_time},
		{"data/shaders/unified.gl",				&unified,			false, graphics::shadow_shader_macro,		empty_file_time},
		{"data/shaders/unified_anim.gl",		&animated_unified,	false, graphics::shadow_shader_macro,		empty_file_time},
		{"data/shaders/water.gl",				&water,				false, graphics::shadow_shader_macro,		empty_file_time},
		{"data/shaders/gaussian_blur.gl",		&gaussian_blur,		false, "",									empty_file_time},
		{"data/shaders/plane_projection.gl",	&plane_projection,	 true, "",									empty_file_time},
		{"data/shaders/jump_flood.gl",			&jfa,				false, "",									empty_file_time},
		{"data/shaders/jfa_to_distance.gl",		&jfa_distance,		false, "",									empty_file_time},
		{"data/shaders/post.gl",				&post[0],			false, "",									empty_file_time},
		{"data/shaders/post.gl",				&post[1],			false, "#define BLOOM 1\n",					empty_file_time},
		{"data/shaders/debug.gl",				&debug,				false, "",									empty_file_time},
		{"data/shaders/depth_only.gl",			&depth_only,		false, "",									empty_file_time},
		{"data/shaders/seaweed.gl",				&vegetation,		false, "",									empty_file_time},
		{"data/shaders/downsample.gl",			&downsample,		false, "",									empty_file_time},
		{"data/shaders/blur_upsample.gl",		&upsample,			false, "",									empty_file_time},
	};
	// Fill in with correct file time and actually load
	for (auto& sd : shader_list) {
		if (std::filesystem::exists(sd.path))
			sd.update_time = std::filesystem::last_write_time(sd.path);

		loadShader(*sd.shader, sd.path, sd.macro, sd.is_geometry);
	}
}

void updateGlobalShaders() {
	for (auto& sd : shader_list) {
		if (std::filesystem::exists(sd.path) && sd.update_time != std::filesystem::last_write_time(sd.path)) {
			sd.update_time = std::filesystem::last_write_time(sd.path);

			loadShader(*sd.shader, sd.path, sd.macro, sd.is_geometry);
		}
	}
}