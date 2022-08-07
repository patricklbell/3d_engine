#ifndef TEXTURE_HPP
#define TEXTURE_HPP

#include <string>
#include <vector>
#include <array>

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

GLuint create1x1Texture(const unsigned char color[3], GLint internal_format=GL_RGB);
GLuint loadImage(const std::string &imagepath, GLint internal_format=GL_RGBA);
GLuint loadCubemap(const std::array<std::string, FACE_NUM_FACES> &paths, GLint internal_format=GL_RGB);

#endif
