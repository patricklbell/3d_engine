#include <cstring>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>

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

namespace shader {
    GLuint null_program;
    struct NullUniforms null_uniforms;

    GLuint unified_programs[2];
    struct UnifiedUniforms unified_uniforms[2];
	bool unified_bloom = false;

	GLuint gaussian_blur_program;
	struct GaussianBlurUniforms gaussian_blur_uniforms;

	GLuint post_program;
}

using namespace shader;

GLuint loadShader(std::string vertex_fragment_file_path, std::string macro_prepends="") {
	const char *path = vertex_fragment_file_path.c_str();
	printf("Loading shader %s.\n", path);
	const char *version_macro  = "#version 330 core \n";
	const char *fragment_macro = "#define COMPILING_FS 1\n";
	const char *vertex_macro   = "#define COMPILING_VS 1\n";
	
	GLuint vertex_shader_id   = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);

	FILE *fp = fopen(path, "r");

	if (fp == NULL) {
		fprintf(stderr, "Can't open shader file %s.\n", path);
		return 0;
	}
	fseek(fp, 0L, SEEK_END);
	int num_bytes = ftell(fp);
	
	// @note adds \0 to fread
	rewind(fp); 
	char *shader_code = (char*)malloc((num_bytes+1) * sizeof(char));	
	if(shader_code == NULL)
		return 0;
	fread(shader_code, sizeof(char), num_bytes, fp);
	fclose(fp);
	shader_code[num_bytes] = 0;
    
	GLint result = GL_FALSE;
	int info_log_length;

	printf("Compiling and linking shader: %s\n", path);

    char *vertex_shader_code[] = {(char*)version_macro, (char*)vertex_macro, (char*)macro_prepends.c_str(), shader_code};

	glShaderSource(vertex_shader_id, 4, vertex_shader_code, NULL);
	glCompileShader(vertex_shader_id);

	glGetShaderiv(vertex_shader_id, GL_INFO_LOG_LENGTH, &info_log_length);
	if ( info_log_length > 0 ){
		char *vertex_shader_error_message = (char *)malloc(sizeof(char) * (info_log_length+1));
		glGetShaderInfoLog(vertex_shader_id, info_log_length, NULL, vertex_shader_error_message);
		fprintf(stderr, "%s\n", vertex_shader_error_message);
		free(vertex_shader_error_message);
    	free(shader_code);
		return GL_FALSE;
	}

	char *fragment_shader_code[] = {(char*)version_macro, (char*)fragment_macro, (char*)macro_prepends.c_str(), shader_code};

	glShaderSource(fragment_shader_id, 4, fragment_shader_code, NULL);
	glCompileShader(fragment_shader_id);

	glGetShaderiv(fragment_shader_id, GL_INFO_LOG_LENGTH, &info_log_length);
	if ( info_log_length > 0 ){
		char *fragment_shader_error_message = (char *)malloc(sizeof(char) * (info_log_length+1));
		glGetShaderInfoLog(fragment_shader_id, info_log_length, NULL, fragment_shader_error_message);
		fprintf(stderr, "%s\n", fragment_shader_error_message);
		free(fragment_shader_error_message);
    	free(shader_code);
		return GL_FALSE;
	}

	GLuint program_id = glCreateProgram();
	glAttachShader(program_id, vertex_shader_id);
	glAttachShader(program_id, fragment_shader_id);
	glLinkProgram(program_id);

	glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &info_log_length);
	if ( info_log_length > 0 ){
		char *program_error_message = (char *)malloc(sizeof(char) * (info_log_length+1));
		glGetProgramInfoLog(program_id, info_log_length, NULL, program_error_message);
		fprintf(stderr, "%s\n", program_error_message);
		free(program_error_message);
    	free(shader_code);
		return GL_FALSE;
	}


	glDetachShader(program_id, vertex_shader_id);
	glDetachShader(program_id, fragment_shader_id);
	
	glDeleteShader(vertex_shader_id);
	glDeleteShader(fragment_shader_id);

	return program_id;
}
void loadPostShader(std::string path){
	auto tmp = post_program;
	// Create and compile our GLSL program from the shaders
	post_program = loadShader(path);
	if(post_program == GL_FALSE) {
		printf("Failed to load post shader\n");
		post_program = tmp;
		return;
	}

	glUseProgram(post_program);
	// Set fixed locations for textures in GL_TEXTUREi
	glUniform1i(glGetUniformLocation(post_program, "pixel_map"), 0);
	glUniform1i(glGetUniformLocation(post_program, "bloom_map"),  1);
}
void loadGaussianBlurShader(std::string path){
	auto tmp = gaussian_blur_program;
	// Create and compile our GLSL program from the shaders
	gaussian_blur_program = loadShader(path);
	if(gaussian_blur_program == GL_FALSE) {
		printf("Failed to load gaussian blur shader\n");
		gaussian_blur_program = tmp;
		return;
	}

	// Grab uniforms to modify
	gaussian_blur_uniforms.horizontal = glGetUniformLocation(gaussian_blur_program, "horizontal");

	glUseProgram(gaussian_blur_program);
	// Set fixed locations for textures in GL_TEXTUREi
	glUniform1i(glGetUniformLocation(gaussian_blur_program, "image"), 0);
}
void loadNullShader(std::string path){
	auto tmp = null_program;
	// Create and compile our GLSL program from the shaders
	null_program = loadShader(path);
	if(null_program == GL_FALSE) {
		printf("Failed to load null shader\n");
		null_program = tmp;
		return;
	}

	// Grab uniforms to modify
	null_uniforms.mvp = glGetUniformLocation(null_program, "mvp");
}
void loadUnifiedShader(std::string path){
	static const std::string macros[] = {"", "#define BLOOM 1\n"};
	for(int i = 0; i < 2; i++){
		// Create and compile our GLSL program from the shaders
		auto tmp = unified_programs[i];
		unified_programs[i] = loadShader(path, macros[i]);
		if(unified_programs[i] == GL_FALSE) {
			unified_programs[i] = tmp;
			return;
		}
		// Grab uniforms to modify during rendering
		unified_uniforms[i].mvp   = glGetUniformLocation(unified_programs[i], "mvp");
		unified_uniforms[i].shadow_mvp   = glGetUniformLocation(unified_programs[i], "shadow_mvp");
		unified_uniforms[i].model = glGetUniformLocation(unified_programs[i], "model");
		unified_uniforms[i].sun_color = glGetUniformLocation(unified_programs[i], "sun_color");
		unified_uniforms[i].sun_direction = glGetUniformLocation(unified_programs[i], "sun_direction");
		unified_uniforms[i].camera_position = glGetUniformLocation(unified_programs[i], "camera_position");
		
		glUseProgram(unified_programs[i]);
		// Set fixed locations for textures in GL_TEXTUREi
		glUniform1i(glGetUniformLocation(unified_programs[i], "albedo_map"), 0);
		glUniform1i(glGetUniformLocation(unified_programs[i], "normal_map"),  1);
		glUniform1i(glGetUniformLocation(unified_programs[i], "metallic_map"),  2);
		glUniform1i(glGetUniformLocation(unified_programs[i], "roughness_map"),  3);
		glUniform1i(glGetUniformLocation(unified_programs[i], "ao_map"),  4);
		glUniform1i(glGetUniformLocation(unified_programs[i], "shadow_map"),  5);
	}
}
void deleteShaderPrograms(){
    glDeleteProgram(null_program);
    glDeleteProgram(unified_programs[0]);
    glDeleteProgram(unified_programs[1]);
    glDeleteProgram(gaussian_blur_program);
	glDeleteProgram(post_program);
}
