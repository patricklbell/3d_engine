#include <cstring>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <iostream>
#include <unordered_set>
#include <filesystem>

// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <GLFW/glfw3.h>

#include "shader.hpp"
#include "utilities.hpp"
#include "graphics.hpp"
#include "globals.hpp"

bool shader_binary_supported = false;
constexpr bool do_shader_caching = false;

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
	Shader animated_null, null, null_vegetation, unified, unified_ns, animated_unified, animated_unified_ns, water, water_ns, gaussian_blur,
		plane_projection, jfa, jfa_distance, post[2], debug, depth_only, vegetation, 
		downsample, upsample, diffuse_convolution, specular_convolution, generate_brdf_lut, skybox;
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
	char *data = (char*)malloc((num_bytes + 2) * sizeof(char));
	if (data == NULL) {
		fclose(fp);
		return nullptr;
	}
	fread(data, sizeof(char), num_bytes, fp);
	fclose(fp);

	// Add newline for glsl compiler
	data[num_bytes] = '\n';
	data[num_bytes + 1] = '\0';

	return data;
}

static inline void free_linked_shader_codes(std::vector<char *> &lsc) {
	for (auto& s : lsc) {
		free(s);
	}
}

static void load_shader_dependencies(char *shader_code, int num_bytes, 
		std::vector<char *> &linked_shader_codes, std::vector<char *>& linked_shader_codes_to_free,
		std::unordered_set<std::string> &linked_shader_paths) {
	int offset = 0;

	constexpr std::string_view load_macro = "#load ";
	for(int line_i = 0; offset < num_bytes; ++line_i) {
		int aoffset;
		auto line = read_line(&shader_code[offset], num_bytes - offset, aoffset);

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
						//std::cout << "Successful #load at path " << loadpath << "\n";
						load_shader_dependencies(loaded_shader, loaded_num_bytes, linked_shader_codes, linked_shader_codes_to_free, linked_shader_paths);
						linked_shader_codes_to_free.push_back(loaded_shader);
						linked_shader_codes.push_back(loaded_shader);	
					} else {
						std::cout << "Failed to #load path " << loadpath << "\n";
					}
				} else {
					//std::cout << "Skipped duplicate #load of path " << loadpath << "\n";
				}
			}
		}
		offset += aoffset;
	}
}

char *load_shader_cache(std::string_view path, uint64_t current_update_time, GLenum &binaryFormat, GLsizei &length) {
	FILE *fp = fopen(path.data(), "rb");

	if (fp == NULL) {
		return nullptr;
	}

	uint64_t stored_time;
	fread(&stored_time, sizeof(stored_time), 1, fp);

	if(stored_time >= current_update_time) {
		fread(&binaryFormat, sizeof(binaryFormat), 1, fp);
		fread(&length, sizeof(length), 1, fp);

		char *data = (char*)malloc(length);
		if (data == NULL) {
			fclose(fp);
			return nullptr;
		}
		fread(data, length, 1, fp);

		fclose(fp);
		return data;
	}
	
	fclose(fp);
	return nullptr;
}

bool write_shader_cache(Shader &shader, std::string_view path, uint64_t update_time) {
	FILE *fp = fopen(path.data(), "wb");

	if (fp == NULL) {
		return false;
	}

	GLsizei length;
	glGetProgramiv(shader.program, GL_PROGRAM_BINARY_LENGTH, &length);
	if(length == 0) {
		fclose(fp);
		return false;
	}
	char *data = (char*)malloc(length);
	if (data == NULL) {
		fclose(fp);
		return false;
	}
	GLenum binary_format;
	glGetProgramBinary(shader.program, length, nullptr, &binary_format, data);

	fwrite(&update_time, sizeof(update_time), 1, fp);
	fwrite(&binary_format, sizeof(binary_format), 1, fp);
	fwrite(&length, sizeof(length), 1, fp);
	fwrite(data, length, 1, fp);

	std::cout << "Wrote shader cache to " << path << ".\n";

	free(data);
	fclose(fp);
	return true;
}

void create_shader_from_program(Shader& shader, GLuint program_id, std::string_view handle) {
	shader.program = program_id;
	shader.handle = std::string(handle);

	GLint count;
	glGetProgramiv(shader.program, GL_ACTIVE_UNIFORMS, &count);
	//printf("Active Uniforms: %d\n", count);

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
}

static bool not_alphanumeric(char c) {
	return !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'));
}

bool loadShader(Shader& shader, std::string_view vertex_fragment_file_path, std::string_view macros="", bool geometry = false) {
	// @todo handle macros in cache properly for now just use a hacky way
	// @fix this doesn't reload a shader if it's dependencies change, maybe bake these in cache file?
	auto cache_path = std::string(vertex_fragment_file_path) + ".cache";
	if (macros != "") {
		std::string m_alpha = std::string(macros);
		m_alpha.erase(std::remove_if(m_alpha.begin(), m_alpha.end(), not_alphanumeric), m_alpha.end());
		cache_path += "." + m_alpha;
	}

	uint64_t unix_update_time;
	if(do_shader_caching && shader_binary_supported && std::filesystem::exists(vertex_fragment_file_path)) {
		auto unix_timestamp = std::chrono::seconds(std::time(NULL));
		auto update_time = std::filesystem::last_write_time(vertex_fragment_file_path).time_since_epoch();
		unix_update_time = (update_time - unix_timestamp).count();

		GLenum binary_format;
		GLsizei binary_length;
		char *cache_data = load_shader_cache(cache_path, unix_update_time, binary_format, binary_length);
		if(cache_data != nullptr) {
			std::cout << "Loading cached shader " << cache_path << ".\n";
			GLuint program_id = glCreateProgram();
			glProgramBinary(program_id, binary_format, cache_data, binary_length);
			free(cache_data);

			GLint status;
			glGetProgramiv(program_id, GL_LINK_STATUS, &status);
			if(status == GL_TRUE) {
				create_shader_from_program(shader, program_id, vertex_fragment_file_path);
				std::cout << "\n";
				return true;
			} else {
				std::cerr << "Failed to load cached shader\n";
				int info_log_length;
				glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &info_log_length);
				if(info_log_length > 0) {
					char *program_error_message = (char *)malloc(sizeof(char) * (info_log_length+1));
					glGetProgramInfoLog(program_id, info_log_length, NULL, program_error_message);
					std::cerr << "Error was:\n" << program_error_message << "\n";
					free(program_error_message);
				}
			}
		}
	}

	const char *path = vertex_fragment_file_path.data();
	std::cout << "Loading shader " << path << ".\n";
	const char *fragment_macro = "#define COMPILING_FS 1\n";	
	const char *vertex_macro   = "#define COMPILING_VS 1\n";
	
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
	std::vector<char *> linked_shader_codes_to_free = { shader_code }; // Pretty cringe way @todo
	load_shader_dependencies(shader_code, num_bytes, linked_shader_codes, linked_shader_codes_to_free, linked_shader_paths);

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

		for (auto &s : linked_shader_codes) {
			std::cerr << s;
		}
		free(vertex_shader_error_message);
		free_linked_shader_codes(linked_shader_codes_to_free);
		return false;
	}

	linked_shader_codes[linked_shader_codes.size() - 2] = (char*)fragment_macro;
	glShaderSource(fragment_shader_id, linked_shader_codes.size(), &linked_shader_codes[0], NULL);
	glCompileShader(fragment_shader_id);

	glGetShaderiv(fragment_shader_id, GL_INFO_LOG_LENGTH, &info_log_length);
	if (info_log_length > 0) {
		char* fragment_shader_error_message = (char*)malloc(sizeof(char) * (info_log_length + 1));
		glGetShaderInfoLog(fragment_shader_id, info_log_length, NULL, fragment_shader_error_message);
		std::cerr << "Fragment shader:\n" << fragment_shader_error_message << "\n";
		for (auto& s : linked_shader_codes) {
			std::cerr << s;
		}
		free(fragment_shader_error_message);
		free_linked_shader_codes(linked_shader_codes_to_free);
		return false;
	}

	GLuint geometry_shader_id;
	if (geometry) {
		std::cout << "Compiling additional geometry shader.\n";
		const char* geometry_macro = "#define COMPILING_GS 1\n";

		geometry_shader_id = glCreateShader(GL_GEOMETRY_SHADER);
		linked_shader_codes[linked_shader_codes.size() - 2] = (char*)geometry_macro;
		glShaderSource(geometry_shader_id, linked_shader_codes.size(), &linked_shader_codes[0], NULL);
		glCompileShader(geometry_shader_id);

		glGetShaderiv(geometry_shader_id, GL_INFO_LOG_LENGTH, &info_log_length);
		if (info_log_length > 0) {
			char* geometry_shader_error_message = (char*)malloc(sizeof(char) * (info_log_length + 1));
			glGetShaderInfoLog(geometry_shader_id, info_log_length, NULL, geometry_shader_error_message);
			std::cerr << "Geometry shader:\n" << geometry_shader_error_message << "\n";
			for (auto& s : linked_shader_codes) {
				std::cerr << s;
			}
			free(geometry_shader_error_message);
			free_linked_shader_codes(linked_shader_codes_to_free);
			return false;
		}
	}

	GLuint program_id = glCreateProgram();
	glAttachShader(program_id, vertex_shader_id);
	if (geometry) glAttachShader(program_id, geometry_shader_id);
	glAttachShader(program_id, fragment_shader_id);
	glLinkProgram(program_id);

	glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &info_log_length);
	if (info_log_length > 0) {
		char* program_error_message = (char*)malloc(sizeof(char) * (info_log_length + 1));
		glGetProgramInfoLog(program_id, info_log_length, NULL, program_error_message);
		std::cerr << "Program attaching:\n" << program_error_message << "\n";
		for (auto& s : linked_shader_codes) {
			std::cerr << s;
		}
		free(program_error_message);
		free_linked_shader_codes(linked_shader_codes_to_free);
		return false;
	}

	glDetachShader(program_id, vertex_shader_id);
	if (geometry) glDetachShader(program_id, geometry_shader_id);
	glDetachShader(program_id, fragment_shader_id);

	glDeleteShader(vertex_shader_id);
	if (geometry) glDeleteShader(geometry_shader_id);
	glDeleteShader(fragment_shader_id);

	create_shader_from_program(shader, program_id, vertex_fragment_file_path);

	free_linked_shader_codes(linked_shader_codes_to_free);

	if(do_shader_caching && shader_binary_supported)
		write_shader_cache(shader, cache_path, unix_update_time);
	
	return true;
}

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
	GLint num_binary_formats;
	glGetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS, &num_binary_formats);
	shader_binary_supported = num_binary_formats > 0;

	std::filesystem::file_time_type empty_file_time;
	auto s = &animated_null;
	shader_list = {
		{"data/shaders/null.gl",				&animated_null,			true,  graphics::shadow_shader_macro+graphics::animation_macro,		empty_file_time},
		{"data/shaders/null.gl",				&null,					true,  graphics::shadow_shader_macro,								empty_file_time},
		{"data/shaders/null.gl",				&null_vegetation,		true,  graphics::shadow_shader_macro+"\n#define VEGETATION 1\n",	empty_file_time},
		{"data/shaders/unified.gl",				&unified,				false, graphics::shadow_shader_macro,								empty_file_time},
		{"data/shaders/unified.gl",				&unified_ns,			false, "#define SHADOWS 0\n",										empty_file_time},
		{"data/shaders/unified.gl",				&animated_unified,		false, graphics::shadow_shader_macro+graphics::animation_macro,		empty_file_time},
		{"data/shaders/unified.gl",				&animated_unified_ns,	false, "#define SHADOWS 0\n"+graphics::animation_macro,				empty_file_time},
		{"data/shaders/water.gl",				&water,					false, graphics::shadow_shader_macro,								empty_file_time},
		{"data/shaders/water.gl",				&water_ns,				false, "#define SHADOWS 0\n",										empty_file_time},
		{"data/shaders/gaussian_blur.gl",		&gaussian_blur,			false, "",															empty_file_time},
		{"data/shaders/plane_projection.gl",	&plane_projection,		true,  "",															empty_file_time},
		{"data/shaders/jump_flood.gl",			&jfa,					false, "",															empty_file_time},
		{"data/shaders/jfa_to_distance.gl",		&jfa_distance,			false, "",															empty_file_time},
		{"data/shaders/post.gl",				&post[0],				false, "",															empty_file_time},
		{"data/shaders/post.gl",				&post[1],				false, "#define BLOOM 1\n",											empty_file_time},
		{"data/shaders/debug.gl",				&debug,					false, "",															empty_file_time},
		{"data/shaders/depth_only.gl",			&depth_only,			false, "",															empty_file_time},
		{"data/shaders/vegetation.gl",			&vegetation,			false, "",															empty_file_time},
		{"data/shaders/downsample.gl",			&downsample,			false, "",															empty_file_time},
		{"data/shaders/blur_upsample.gl",		&upsample,				false, "",															empty_file_time},
		{"data/shaders/diffuse_convolution.gl",	&diffuse_convolution,	false, "",															empty_file_time},
		{"data/shaders/specular_convolution.gl",&specular_convolution,	false, "",															empty_file_time},
		{"data/shaders/generate_brdf_lut.gl",   &generate_brdf_lut,	    false, "",															empty_file_time},
		{"data/shaders/skybox.gl",				&skybox,				false, "",															empty_file_time},
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
