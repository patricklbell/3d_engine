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
    GLuint geometry;
    struct GeometryUniforms geometry_uniforms;
    GLuint post;
    struct PostUniforms post_uniforms;
    GLuint point;
    struct PointUniforms point_uniforms;
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
		printf("Can't open shader %s.\n", path);
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

	printf("Compiling and linking shader : %s\n", path);

    char *vertex_shader_code[] = {(char*)version_macro, (char*)vertex_macro, shader_code};

	glShaderSource(vertex_shader_id, 3, vertex_shader_code, NULL);
	glCompileShader(vertex_shader_id);

	// Check vertex shader compilation
	glGetShaderiv(vertex_shader_id, GL_INFO_LOG_LENGTH, &info_log_length);
	if ( info_log_length > 0 ){
		char *vertex_shader_error_messages[info_log_length+1];
		glGetShaderInfoLog(vertex_shader_id, info_log_length, NULL, vertex_shader_error_messages[0]);
		printf("%s\n", vertex_shader_error_messages[0]);
	}

    char *fragment_shader_code[] = {(char*)version_macro, (char*)fragment_macro, shader_code};

	glShaderSource(fragment_shader_id, 3, fragment_shader_code, NULL);
	glCompileShader(fragment_shader_id);

	glGetShaderiv(fragment_shader_id, GL_INFO_LOG_LENGTH, &info_log_length);
	if ( info_log_length > 0 ){
		char *fragment_shader_error_messages[info_log_length+1];
		glGetShaderInfoLog(fragment_shader_id, info_log_length, NULL, fragment_shader_error_messages[0]);
		printf("%s\n", fragment_shader_error_messages[0]);
	}

	GLuint program_id = glCreateProgram();
	glAttachShader(program_id, vertex_shader_id);
	glAttachShader(program_id, fragment_shader_id);
	glLinkProgram(program_id);

	glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &info_log_length);
	if ( info_log_length > 0 ){
		char *program_error_message[info_log_length+1];
		glGetProgramInfoLog(program_id, info_log_length, NULL, program_error_message[0]);
		printf("%s\n", program_error_message[0]);
	}

	glDetachShader(program_id, vertex_shader_id);
	glDetachShader(program_id, fragment_shader_id);
	
	glDeleteShader(vertex_shader_id);
	glDeleteShader(fragment_shader_id);

    free(shader_code);

	return program_id;
}

void loadGeometryShader(std::string path){
	// Create and compile our GLSL program from the shaders
	GLuint program_id = loadShader(path);
	geometry = program_id;

	// Grab geom uniforms to modify
	geometry_uniforms.mvp   = glGetUniformLocation(program_id, "MVP");
	geometry_uniforms.model = glGetUniformLocation(program_id, "model");

	glUseProgram(program_id);
	// Set fixed locations for textures
	glUniform1i(glGetUniformLocation(program_id, "diffuseMap"), 0);
	glUniform1i(glGetUniformLocation(program_id, "normalMap"),  1);
}

void loadPostShader(std::string path){
	GLuint program_id = loadShader(path);
	post = program_id;

	post_uniforms.screen_size = glGetUniformLocation(program_id, "screenSize");
	post_uniforms.light_color = glGetUniformLocation(program_id, "lightColor");
	post_uniforms.light_direction = glGetUniformLocation(program_id, "lightDirection");
	post_uniforms.camera_position = glGetUniformLocation(program_id, "cameraPosition");

	glUseProgram(program_id);
	glUniformMatrix4fv(glGetUniformLocation(program_id, "MVP"), 1, GL_FALSE, &glm::mat4()[0][0]);
	// Set fixed locations for frame buffer
	glUniform1i(glGetUniformLocation(program_id, "positionMap"), GBuffer::GBUFFER_TEXTURE_TYPE_POSITION);
	glUniform1i(glGetUniformLocation(program_id, "normalMap"), GBuffer::GBUFFER_TEXTURE_TYPE_NORMAL);
	glUniform1i(glGetUniformLocation(program_id, "diffuseMap"), GBuffer::GBUFFER_TEXTURE_TYPE_DIFFUSE);
}
void loadPointLightShader(std::string path){
	GLuint program_id = loadShader(path);
	point = program_id;

	glUseProgram(program_id);
	glUniformMatrix4fv(glGetUniformLocation(program_id, "MVP"), 1, GL_FALSE, &glm::mat4()[0][0]);
	// Set fixed locations for frame buffer
	glUniform1i(glGetUniformLocation(program_id, "positionMap"), GBuffer::GBUFFER_TEXTURE_TYPE_POSITION);
	glUniform1i(glGetUniformLocation(program_id, "normalMap"), GBuffer::GBUFFER_TEXTURE_TYPE_NORMAL);
	glUniform1i(glGetUniformLocation(program_id, "diffuseMap"), GBuffer::GBUFFER_TEXTURE_TYPE_DIFFUSE);
}

void deleteShaderPrograms(){
    glDeleteProgram(geometry);
    glDeleteProgram(post);
    glDeleteProgram(point);
}
