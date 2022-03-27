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

    GLuint unified_program;
    struct UnifiedUniforms unified_uniforms;
}

using namespace shader;

GLuint loadShader(std::string vertex_fragment_file_path) {
	const char *path = vertex_fragment_file_path.c_str();
	printf("Loading shader %s.\n", path);
	const char *version_macro  = "#version 330 core \n";
	const char *fragment_macro = "#define COMPILING_FS 1\n";
	const char *vertex_macro   = "#define COMPILING_VS 1\n";
	
	GLuint vertex_shader_id   = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);

	// Read unified shader code from file
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

    char *vertex_shader_code[] = {(char*)version_macro, (char*)vertex_macro, shader_code};

	glShaderSource(vertex_shader_id, 3, vertex_shader_code, NULL);
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

	char *fragment_shader_code[] = {(char*)version_macro, (char*)fragment_macro, shader_code};

	glShaderSource(fragment_shader_id, 3, fragment_shader_code, NULL);
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

	printf("Loaded shader %s.\n", path);

	return program_id;
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
	// Create and compile our GLSL program from the shaders
	auto tmp = unified_program;
	unified_program = loadShader(path);
	if(unified_program == GL_FALSE) {
		printf("Failed to load unified shader\n");
		unified_program = tmp;
		return;
	}

	// Grab uniforms to modify during rendering
	unified_uniforms.mvp   = glGetUniformLocation(unified_program, "mvp");
	unified_uniforms.model = glGetUniformLocation(unified_program, "model");
	unified_uniforms.sun_color = glGetUniformLocation(unified_program, "sun_color");
	unified_uniforms.sun_direction = glGetUniformLocation(unified_program, "sun_direction");
	unified_uniforms.camera_position = glGetUniformLocation(unified_program, "camera_position");
	
	glUseProgram(unified_program);
	// Set fixed locations for textures in GL_TEXTUREi
	glUniform1i(glGetUniformLocation(unified_program, "albedo_map"), 0);
	glUniform1i(glGetUniformLocation(unified_program, "normal_map"),  1);
	glUniform1i(glGetUniformLocation(unified_program, "metallic_map"),  2);
	glUniform1i(glGetUniformLocation(unified_program, "roughness_map"),  3);
	glUniform1i(glGetUniformLocation(unified_program, "ao_map"),  4);
}
void deleteShaderPrograms(){
    glDeleteProgram(null_program);
    glDeleteProgram(unified_program);
}
