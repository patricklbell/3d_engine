#ifndef GLOBALS_H
#define GLOBALS_H

#include <string>

#include <glm/glm.hpp>

#include <GLFW/glfw3.h>

#include <btBulletDynamicsCommon.h>

#include "imgui.h"

#define ENTITY_COUNT 1000

extern int selected_entity;
extern GLFWwindow* window;
extern std::string glsl_version;
extern btDiscreteDynamicsWorld* dynamics_world;
extern glm::vec3 sun_color;
extern glm::vec3 sun_direction;
extern glm::vec4 clear_color;
extern bool draw_bloom;

#endif
