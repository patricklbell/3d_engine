#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <filesystem>
#include <vector>
#include <iostream>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "texture.hpp"

GLuint create1x1Texture(const unsigned char color[3], GLint internal_format){
	GLuint texture_id;
	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glTexImage2D(GL_TEXTURE_2D, 0, internal_format, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, &color[0]);
	return texture_id;
}

GLuint loadImage(const std::string &imagepath, GLint internal_format) {
	std::cout << "Loading texture at path " << imagepath << "\n";

	int x, y, n;
	unsigned char *data = stbi_load(imagepath.c_str(), &x, &y, &n, 3);

	if(data == NULL){
		std::cerr << "Failed to load texture.\n";
        stbi_image_free(data);
		return GL_FALSE;
	} 

	GLuint texture_id;
	glGenTextures(1, &texture_id);
	
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glTexImage2D(GL_TEXTURE_2D, 0, internal_format, x, y, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
	stbi_image_free(data);

	// Poor filtering, or ...
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 

	// ... nice trilinear filtering ...
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glGenerateMipmap(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, 0);

	return texture_id;
}

GLuint loadCubemap(const std::array<std::string, FACE_NUM_FACES> &paths, GLint internal_format) {
	GLuint texture_id;

    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture_id);

	std::cout << "Loading Cubemap:\n";

    int width, height, nrChannels;
    for (int i = 0; i < static_cast<int>(FACE_NUM_FACES); ++i) {
		std::cout << "\tLoading texture at path " << paths[i] << "\n";
        unsigned char *data = stbi_load(paths[i].c_str(), &width, &height, &nrChannels, 0);
        if (data) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 
                         0, GL_RGB, width, height, 0, internal_format, GL_UNSIGNED_BYTE, data
            );
        } else {
			std::cerr << "Cubemap texture failed to load.\n";
        	stbi_image_free(data);
        	glDeleteTextures(1, &texture_id);
            return GL_FALSE;
        }
        stbi_image_free(data);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	return texture_id;
}
