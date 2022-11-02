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

// Determine how many channels we need to load from the file to fit the format required
ImageChannels getChannelsForFormat(GLenum format) {
	GLint param;

	int channels = 0;

	glGetInternalformativ(GL_TEXTURE_2D, format, GL_INTERNALFORMAT_RED_SIZE, 1, &param);
	channels += param > 0;
	glGetInternalformativ(GL_TEXTURE_2D, format, GL_INTERNALFORMAT_GREEN_SIZE, 1, &param);
	channels += param > 0;
	glGetInternalformativ(GL_TEXTURE_2D, format, GL_INTERNALFORMAT_BLUE_SIZE, 1, &param);
	channels += param > 0;
	glGetInternalformativ(GL_TEXTURE_2D, format, GL_INTERNALFORMAT_ALPHA_SIZE, 1, &param);
	channels += param > 0;

	return ImageChannels(channels);
}

// Returns a format that can matches the number of channels
GLenum getFormatForChannels(ImageChannels channels) {
	static const GLenum formats[] = { GL_RED, GL_RG, GL_RGB, GL_RGBA }; 
	return formats[(int)channels - 1]; // Intentionally throws an exception if channels is invalid
}

GLuint create1x1Texture(const unsigned char color[4], const GLenum format){
	GLuint texture_id;
	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glTexImage2D(GL_TEXTURE_2D, 0, format, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &color[0]);
	return texture_id;
}

GLuint create1x1TextureFloat(const glm::fvec4 &color, const GLenum format) {
	GLuint texture_id;
	glGenTextures(1, &texture_id);
	glBindTexture(GL_TEXTURE_2D, texture_id);
	glTexImage2D(GL_TEXTURE_2D, 0, format, 1, 1, 0, GL_RGBA, GL_FLOAT, &color[0]);
	return texture_id;
}

bool loadImageData(ImageData *img, std::string_view imagepath, ImageChannels channels, bool floating, bool flip) {
	stbi_set_flip_vertically_on_load(flip); // Opengl expects image to be flipped vertically
	if (floating) {
		img->data = (unsigned char*)stbi_loadf(imagepath.data(), &img->x, &img->y, (int*)&img->n, (int)channels);
	}
	else {
		img->data = stbi_load(imagepath.data(), &img->x, &img->y, (int*)&img->n, (int)channels);
	}
	// @todo determine image format from channels and floating point nature
	img->n = channels;
	img->floating = floating;

	if (img->data == NULL) {
		std::cerr << "Failed to load image at path " << imagepath << "\n";
		return false;
	}

	std::cout << "Loaded texture bytes at path " << imagepath << "\n";
	return true;
}

GLuint createGLTextureFromData(ImageData* img, const GLenum format, const GLint wrap, bool trilinear) {
	std::cout << "Created a gl texture.\n";
	// @note mostly redundant
	if (img->data == NULL) 
		return GL_FALSE;

	GLuint texture_id;
	glGenTextures(1, &texture_id);

	glBindTexture(GL_TEXTURE_2D, texture_id);
	// @note not needed if image is in correct format
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, format, img->x, img->y, 0, getFormatForChannels(img->n), img->floating ? GL_FLOAT: GL_UNSIGNED_BYTE, img->data);
	stbi_image_free(img->data);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

	if (trilinear) {
		// Nice trilinear filtering, or ...
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	}
	else {
		// poor filtering
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
	}

	glGenerateMipmap(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, 0);

	return texture_id;
}

GLuint loadImage(std::string_view imagepath, glm::ivec2& resolution, const GLenum format, const GLint wrap, bool floating, bool trilinear) {
	auto img = ImageData();

	if (!loadImageData(&img, imagepath, getChannelsForFormat(format), floating)) {
		stbi_image_free(img.data);
		return GL_FALSE;
	}
	resolution = glm::ivec2(img.x, img.y);

	return createGLTextureFromData(&img, format, wrap, trilinear);
}

// std array is expected to list {front, back, up, down, right, left}?
GLuint loadCubemap(const std::array<std::string, FACE_NUM_FACES> &paths, glm::ivec2& resolution, const GLenum format, const GLint wrap, bool floating, bool trilinear) {
	GLuint texture_id;

    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture_id);

	std::cout << "Loading Cubemap:\n";

	ImageData img;
    for (int i = 0; i < static_cast<int>(FACE_NUM_FACES); ++i) {
		std::cout << "\tLoading texture at path " << paths[i] << "\n";
		// @note I don't know why cubemap is not flipped but it isn't
		if (loadImageData(&img, paths[i], getChannelsForFormat(format), floating, false)) {
			// @note not needed if image is in correct format
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+i, 0, format, img.x, img.y, 0, getFormatForChannels(img.n), img.floating ? GL_FLOAT : GL_UNSIGNED_BYTE, img.data);
        } else {
			stbi_image_free(img.data);
			std::cerr << "Cubemap texture failed to load.\n";
        	glDeleteTextures(1, &texture_id);
            return GL_FALSE;
        }
        stbi_image_free(img.data);
    }
	resolution = glm::ivec2(img.x, img.y);

	if (trilinear) {
		// Nice trilinear filtering, or ...
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	}
	else {
		// Poor filtering
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, wrap);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, wrap);

	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	return texture_id;
}
