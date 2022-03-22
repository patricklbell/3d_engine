#ifndef TEXTURE_HPP
#define TEXTURE_HPP

#include <string>
#include <vector>

#include <GLFW/glfw3.h>

GLuint loadImage(std::string imagepath, bool is_srgb=false);
GLuint loadCubemap(std::vector<std::string> filenames);

#endif
