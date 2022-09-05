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
    extern bool left_mouse_click_release;
    extern bool left_mouse_click_press;

    extern glm::dvec2 mouse_position;
    extern glm::dvec2 delta_mouse_position;
}
void handleEditorControls(Camera& editor_camera, Camera& level_camera, EntityManager& entity_manager, AssetManager &asset_manager, float dt);
void handleGameControls(Camera& camera, EntityManager& entity_manager, AssetManager& asset_manager, float dt);
void initEditorControls();

#endif
