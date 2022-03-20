#include <stdio.h>
#include <string>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>

#include <GL/glew.h>

#include "utilities.hpp"
#include "shader.hpp"

GLuint loadShader(const char * vertex_fragment_file_path) {
	const char *version_macro  = "#version 330 core \n";
	const char *fragment_macro = "#define COMPILING_FS 1\n";
	const char *vertex_macro   = "#define COMPILING_VS 1\n";
	
	GLuint vertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

	// Read unified shader code from file
	FILE *fp = fopen(vertex_fragment_file_path, "r");

	if (fp == NULL) {
		printf("Can't open shader %s.\n", vertex_fragment_file_path);
		return 0;
	}
	fseek(fp, 0L, SEEK_END);
	int num_bytes = ftell(fp);
	 
	/* reset the file position indicator to 
	the beginning of the file */
	fseek(fp, 0L, SEEK_SET);	
	 
	char *shader_code = (char*)calloc(num_bytes, sizeof(char));	
	if(shader_code == NULL)
		return 0;
	fread(shader_code, sizeof(char), num_bytes, fp);
	fclose(fp);

	char *fragment_shader_code[] = {(char *)version_macro, (char *)fragment_macro, shader_code};
	char *vertex_shader_code[]   = {(char *)version_macro, (char *)vertex_macro, shader_code};

	GLint Result = GL_FALSE;
	int InfoLogLength;


	// Compile Vertex Shader
	printf("Compiling shader : %s\n", vertex_file_path);
	char const * VertexSourcePointer = VertexShaderCode.c_str();
	glShaderSource(vertexShaderID, 1, &VertexSourcePointer , NULL);
	glCompileShader(vertexShaderID);

	// Check Vertex Shader
	glGetShaderiv(vertexShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(vertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if ( InfoLogLength > 0 ){
		std::vector<char> VertexShaderErrorMessage(InfoLogLength+1);
		glGetShaderInfoLog(vertexShaderID, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
		printf("%s\n", &VertexShaderErrorMessage[0]);
	}



	// Compile Fragment Shader
	printf("Compiling shader : %s\n", fragment_file_path);
	char const * FragmentSourcePointer = FragmentShaderCode.c_str();
	glShaderSource(fragmentShaderID, 1, &FragmentSourcePointer , NULL);
	glCompileShader(fragmentShaderID);

	// Check Fragment Shader
	glGetShaderiv(fragmentShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(fragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if ( InfoLogLength > 0 ){
		std::vector<char> FragmentShaderErrorMessage(InfoLogLength+1);
		glGetShaderInfoLog(fragmentShaderID, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]);
		printf("%s\n", &FragmentShaderErrorMessage[0]);
	}



	// Link the program
	printf("Linking program\n");
	GLuint ProgramID = glCreateProgram();
	glAttachShader(ProgramID, vertexShaderID);
	glAttachShader(ProgramID, fragmentShaderID);
	glLinkProgram(ProgramID);

	// Check the program
	glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
	glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if ( InfoLogLength > 0 ){
		std::vector<char> ProgramErrorMessage(InfoLogLength+1);
		glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
		printf("%s\n", &ProgramErrorMessage[0]);
	}

	
	glDetachShader(ProgramID, vertexShaderID);
	glDetachShader(ProgramID, fragmentShaderID);
	
	glDeleteShader(vertexShaderID);
	glDeleteShader(fragmentShaderID);

	return ProgramID;
}

GLuint load_geometry_shader(const char *path){
	// Create and compile our GLSL program from the shaders
	GLuint programID = load_shader(path);

	// Grab geom uniforms to modify
	GLuint u_geom_MVP = glGetUniformLocation(programID, "MVP");
	GLuint u_geom_model = glGetUniformLocation(programID, "model");

	glUseProgram(programID);
	// Fixed locations for textures
	glUniform1i(glGetUniformLocation(programID, "diffuseMap"), 0);
	glUniform1i(glGetUniformLocation(programID, "normalMap"),  1);
}
GLuint load_dir_light_shader(const char *path){
	GLuint programID = load_shader(path);
	GLuint u_dir_screen_size = glGetUniformLocation(directionalProgramID, "screenSize");
	GLuint u_dir_light_color = glGetUniformLocation(directionalProgramID, "lightColor");
	GLuint u_dir_light_direction = glGetUniformLocation(directionalProgramID, "lightDirection");
	GLuint u_dir_camera_position = glGetUniformLocation(directionalProgramID, "cameraPosition");

	glUseProgram(programID);
	glUniformMatrix4fv(glGetUniformLocation(programID, "MVP"), 1, GL_FALSE, &IDENTITY[0][0]);
	glUniform1i(glGetUniformLocation(programID, "positionMap"), GBuffer::GBUFFER_TEXTURE_TYPE_POSITION);
	glUniform1i(glGetUniformLocation(programID, "normalMap"), GBuffer::GBUFFER_TEXTURE_TYPE_NORMAL);
	glUniform1i(glGetUniformLocation(programID, "diffuseMap"), GBuffer::GBUFFER_TEXTURE_TYPE_DIFFUSE);
}
else if(program.first == "pointProgramID"){
	pointProgramID = load_shader("data/shaders/light_pass.vert", "data/shaders/point_light_pass.frag",false);

	glUseProgram(pointProgramID);
	glUniformMatrix4fv(glGetUniformLocation(pointProgramID, "MVP"), 1, GL_FALSE, &IDENTITY[0][0]);
	glUniform1i(glGetUniformLocation(pointProgramID, "positionMap"), GBuffer::GBUFFER_TEXTURE_TYPE_POSITION);
	glUniform1i(glGetUniformLocation(pointProgramID, "normalMap"), GBuffer::GBUFFER_TEXTURE_TYPE_NORMAL);
	glUniform1i(glGetUniformLocation(pointProgramID, "diffuseMap"), GBuffer::GBUFFER_TEXTURE_TYPE_DIFFUSE);
}

}


