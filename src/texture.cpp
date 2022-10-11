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

GLuint create1x1Texture(const unsigned char color[3], const GLint internal_format){
	GLuint texture_id;
	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glTexImage2D(GL_TEXTURE_2D, 0, internal_format, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, &color[0]);
	return texture_id;
}

GLuint create1x1TextureFloat(const glm::fvec3 &color, const GLint internal_format) {
	GLuint texture_id;
	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glTexImage2D(GL_TEXTURE_2D, 0, internal_format, 1, 1, 0, GL_RGB, GL_FLOAT, &color[0]);
	return texture_id;
}

bool loadImageData(ImageData *img, const std::string& imagepath, const GLint internal_format) {
	if (internal_format == GL_RGB)			img->data = stbi_load(imagepath.c_str(), &img->x, &img->y, &img->n, STBI_rgb);
	else if (internal_format == GL_RED)		img->data = stbi_load(imagepath.c_str(), &img->x, &img->y, &img->n, STBI_grey);
	else if (internal_format == GL_RGB32F)	img->data = (unsigned char*)stbi_loadf(imagepath.c_str(), &img->x, &img->y, &img->n, STBI_rgb);
	else									img->data = stbi_load(imagepath.c_str(), &img->x, &img->y, &img->n, STBI_rgb_alpha);

	if (img->data == NULL) {
		std::cerr << "Failed to load image at path " << imagepath << "\n";
		return false;
	}

	std::cout << "Loaded texture bytes at path " << imagepath << "\n";
	return true;
}

GLuint createGLTextureFromData(ImageData *img, const GLint internal_format) {
	std::cout << "Created a gl texture.\n";
	// @note mostly redundant
	if (img->data == NULL) 
		return GL_FALSE;

	GLuint texture_id;
	glGenTextures(1, &texture_id);

	glBindTexture(GL_TEXTURE_2D, texture_id);
	// @note not needed if image is in correct format
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	if (internal_format == GL_RGB)				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, img->x, img->y, 0, GL_RGB,  GL_UNSIGNED_BYTE, img->data);
	else if (internal_format == GL_RED)			glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, img->x, img->y, 0, GL_RED,  GL_UNSIGNED_BYTE,   img->data);
	else if (internal_format == GL_RGB32F)		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, img->x, img->y, 0, GL_RGB, GL_FLOAT,        img->data);
	else										glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, img->x, img->y, 0, GL_RGBA, GL_UNSIGNED_BYTE, img->data);

	stbi_image_free(img->data);

	// Poor filtering, or ...
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 

	// ... nice trilinear filtering ...
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glGenerateMipmap(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, 0);

	return texture_id;
}

GLuint loadImage(const std::string &imagepath, glm::ivec2& resolution, const GLint internal_format) {
	auto img = ImageData();
	img.internal_format = internal_format;
	if (!loadImageData(&img, imagepath, internal_format)) {
		stbi_image_free(img.data);
		return GL_FALSE;
	}
	resolution = glm::ivec2(img.x, img.y);

	return createGLTextureFromData(&img, internal_format);
}

// std array is expected to list {front, back, up, down, right, left}?
GLuint loadCubemap(const std::array<std::string, FACE_NUM_FACES> &paths, glm::ivec2& resolution, const GLint internal_format) {
	GLuint texture_id;

    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture_id);

	std::cout << "Loading Cubemap:\n";

	int x, y, n;
	unsigned char* data;
    for (int i = 0; i < static_cast<int>(FACE_NUM_FACES); ++i) {
		std::cout << "\tLoading texture at path " << paths[i] << "\n";
		if (internal_format == GL_RGB)			data = stbi_load(paths[i].c_str(), &x, &y, &n, STBI_rgb);
		else if (internal_format == GL_RED)		data = stbi_load(paths[i].c_str(), &x, &y, &n, STBI_grey);
		else if (internal_format == GL_RGB32F)	data = (unsigned char*)stbi_loadf(paths[i].c_str(), &x, &y, &n, STBI_rgb);
		else									data = stbi_load(paths[i].c_str(), &x, &y, &n, STBI_rgb_alpha);
        if (data) {
			// @note not needed if image is in correct format
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			if (internal_format == GL_RGB)			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internal_format, x, y, 0, GL_RGB , GL_UNSIGNED_BYTE, data);
			else if (internal_format == GL_RED)		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internal_format, x, y, 0, GL_RED , GL_UNSIGNED_BYTE, data);
			else if (internal_format == GL_RGB32F)	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internal_format, x, y, 0, GL_RGB32F, GL_FLOAT,		 data);
			else									glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internal_format, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        } else {
			std::cerr << "Cubemap texture failed to load.\n";
        	glDeleteTextures(1, &texture_id);
            return GL_FALSE;
        }
        stbi_image_free(data);
    }
	resolution = glm::ivec2(x, y);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	return texture_id;
}
