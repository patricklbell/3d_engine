#ifndef TEXTURE_HPP
#define TEXTURE_HPP

#include <string>
#include <vector>
#include <array>

#include <glm/glm.hpp>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

enum CubemapFaces : unsigned int {
    FACE_RIGHT = 0,
    FACE_LEFT, 
    FACE_TOP,
    FACE_BOTTOM,
    FACE_FRONT,
    FACE_BACK,
    FACE_NUM_FACES,
};


enum class ImageChannels: int {
    RED = 1,
    RG = 2,
    RGB = 3,
    RGBA = 4,
};

struct ImageData {
	unsigned char* data;
	int x, y;
    ImageChannels n;
    bool floating;
};

ImageChannels getChannelsForFormat(GLenum format);
GLenum getFormatForChannels(ImageChannels channels);

GLuint create1x1Texture(const unsigned char color[4], const GLenum format = GL_RGB);
GLuint create1x1TextureFloat(const glm::fvec4& color, const GLenum format = GL_RGB);
bool loadImageData(ImageData* img, std::string_view imagepath, ImageChannels channels, bool floating = false, bool flip = true);
GLuint createGLTextureFromData(ImageData* img, const GLenum format = GL_RGBA, const GLint wrap = GL_REPEAT, bool trilinear = true);
GLuint loadImage(std::string_view imagepath, glm::ivec2& resolution, const GLenum format=GL_RGBA, const GLint wrap = GL_REPEAT, bool floating = false, bool trilinear = true);
GLuint loadCubemap(const std::array<std::string, FACE_NUM_FACES> &paths, glm::ivec2& resolution, 
                   const GLenum format=GL_RGBA, const GLint wrap = GL_REPEAT, bool floating = false, bool trilinear = true);

#endif
