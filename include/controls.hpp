#ifndef CONTROLS_H
#define CONTROLS_H

#include "glm/detail/type_vec.hpp"

#include <GLFW/glfw3.h>

#include "utilities.hpp"
#include "globals.hpp"
#include "graphics.hpp"

void windowScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

namespace controls {
    extern glm::vec2 scroll_offset;
    extern bool scrolled;

    extern glm::dvec2 mouse_position;
    extern glm::dvec2 delta_mouse_position;
}
void handleEditorControls(Camera &camera, Entity *entities[ENTITY_COUNT], float dt);
void initEditorControls();

#endif
