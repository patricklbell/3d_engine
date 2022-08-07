#ifndef GLOBALS_H
#define GLOBALS_H

#include <string>

#include <glm/glm.hpp>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "assets.hpp"

#define ENTITY_COUNT 1000
#define PI           3.14159265359

// disable Windows.h min max macros
#ifdef _WINDOWS
#define NOMINMAX 
#endif

extern GLFWwindow* window;
extern std::string glsl_version;
extern std::string exepath;
extern glm::vec3 sun_color;
extern glm::vec3 sun_direction;
extern glm::vec4 clear_color;
extern bool draw_bloom;
extern AssetManager global_assets;
extern std::string GL_version, GL_vendor, GL_renderer;

#endif
