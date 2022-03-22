#include <iostream>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Include ImGui
#include "glm/detail/func_geometric.hpp"
#include "imgui.h"

#include "controls.hpp"
#include "graphics.hpp"
#include "utilities.hpp"
#include "globals.hpp"


namespace controls {
    glm::vec2 scroll_offset;
    bool scrolled;

    glm::dvec2 mouse_position;
    glm::dvec2 delta_mouse_position;
}

using namespace controls;

void windowScrollCallback(GLFWwindow* window, double xoffset, double yoffset){
    if(scroll_offset.x != xoffset || scroll_offset.y != yoffset) scrolled = true; 
    else scrolled = false;

    scroll_offset.x = xoffset;
    scroll_offset.y = yoffset;
}

void initEditorControls(){
    glfwGetCursorPos(window, &mouse_position.x, &mouse_position.y);
    delta_mouse_position = glm::dvec2(0,0);
}

void handleEditorControls(Camera &camera, Entity *entities[ENTITY_COUNT], float dt) {
    // Stores the previous state of input, updated at end of function
    static int space_state = GLFW_RELEASE;
    static int mouse_left_state = GLFW_RELEASE;
    static double mouse_left_press_time = glfwGetTime();

    // Unlike other inputs, calculate delta but update mouse position immediately
    glm::dvec2 delta_mouse_position = mouse_position;
    glfwGetCursorPos(window, &mouse_position.x, &mouse_position.y);
    delta_mouse_position = mouse_position - delta_mouse_position;

    if(glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && space_state == GLFW_RELEASE){
        if(camera.state == Camera::TYPE::TRACKBALL) {
            camera.state = Camera::TYPE::SHOOTER;

            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            glfwSetCursorPos(window, window_width/2, window_height/2);

            mouse_position = glm::dvec2(window_width/2, window_height/2);
            delta_mouse_position = glm::dvec2(0,0);

            selected_entity = -1;
        } else if(camera.state == Camera::TYPE::SHOOTER) {
            camera.state = Camera::TYPE::TRACKBALL;

            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
           
            if(selected_entity != -1){
                camera.target = glm::vec3(entities[selected_entity]->transform[3]);
                updateCameraView(camera);
            }
        }
    }

    ImGuiIO& io = ImGui::GetIO();
    if(camera.state == Camera::TYPE::TRACKBALL){
        if (!io.WantCaptureMouse && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE && mouse_left_state == GLFW_PRESS && (glfwGetTime() - mouse_left_press_time) < 0.2) {
            glm::vec3 out_origin;
            glm::vec3 out_direction;
            screenPosToWorldRay(mouse_position, camera.view, camera.projection, out_origin, out_direction);

            glm::vec3 out_end = out_origin + out_direction * 1000.0f;
            glm::vec3 out_end_test = out_origin + out_direction * 2.0f;

            btCollisionWorld::ClosestRayResultCallback RayCallback(btVector3(out_origin.x, out_origin.y, out_origin.z), btVector3(out_end.x, out_end.y, out_end.z));
            dynamics_world->rayTest(btVector3(out_origin.x, out_origin.y, out_origin.z), btVector3(out_end.x, out_end.y, out_end.z), RayCallback);

            if (RayCallback.hasHit()) {
                selected_entity = RayCallback.m_collisionObject->getUserIndex();
                if (entities[selected_entity] == nullptr)
                    selected_entity = -1;
                std::cout << "Selected game object " << selected_entity << "\n";
                // Maintain distance position of old selected entity
                //camera.position = glm::vec3(entities[selected_entity]->transform[3]) + camera.position - camera.target;
                camera.target = glm::vec3(entities[selected_entity]->transform[3]);
                updateCameraView(camera);
            } else {
                selected_entity = -1;
            }
        } else if (!io.WantCaptureMouse && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            auto camera_right = glm::vec3(glm::transpose(camera.view)[0]);

            // Calculate the amount of rotation given the mouse movement.
            float delta_angle_x = (2 * M_PI / (float)window_width); // a movement from left to right = 2*PI = 360 deg
            float delta_angle_y = (M_PI / (float)window_height);  // a movement from top to bottom = PI = 180 deg
            float x_angle = -delta_mouse_position.x * delta_angle_x;
            float y_angle = -delta_mouse_position.y * delta_angle_y;

            // @todo Handle camera passing over poles of orbit 

            // Rotate the camera around the pivot point on the first axis.
            auto rotation_x = glm::rotate(glm::mat4x4(1.0f), x_angle, camera.up);
            auto camera_position_rotated = glm::vec3(rotation_x * glm::vec4(camera.position - camera.target, 1)) + camera.target;

            // Rotate the camera around the pivot point on the second axis.
            auto rotation_y = glm::rotate(glm::mat4x4(1.0f), y_angle, camera_right);
            camera_position_rotated = glm::vec3(rotation_y * glm::vec4(camera_position_rotated - camera.target, 1)) + camera.target;

            // Update the camera view
            camera.position = camera_position_rotated;
            updateCameraView(camera);
        }
        if(!io.WantCaptureMouse && scroll_offset.y != 0){
            float distance = glm::length(camera.position - camera.target);
            distance = abs(distance + distance*scroll_offset.y*0.1);

            camera.position = camera.target + glm::normalize(camera.position - camera.target)*distance;
            updateCameraView(camera);

            // Handle scroll event
            scroll_offset.y = 0;
        }
    } else if (camera.state == Camera::TYPE::SHOOTER){
        static const float camera_movement_speed = 2.0;

        glm::vec3 camera_direction = glm::normalize(camera.target - camera.position);

        // Calculate the amount of rotation given the mouse movement.
        float delta_angle_x = (2 * M_PI / (float)window_width); // a movement from left to right = 2*PI = 360 deg
        float delta_angle_y = (M_PI / (float)window_height);  // a movement from top to bottom = PI = 180 deg
        float x_angle = -delta_mouse_position.x * delta_angle_x;
        float y_angle = -delta_mouse_position.y * delta_angle_y;

        // @todo Handle camera passing over poles of orbit 
        
        // Rotate the camera around the pivot point on the first axis.
        auto rotation_x = glm::rotate(glm::mat4x4(1.0f), x_angle, camera.up);
        auto camera_right = glm::vec3(glm::transpose(camera.view)[0]);
        auto rotation_y = glm::rotate(glm::mat4x4(1.0f), y_angle, camera_right);

        glm::vec3 camera_direction_rotated = glm::vec3(rotation_y * (rotation_x * glm::vec4(camera_direction, 1)));

        // Move forward
        if (glfwGetKey( window, GLFW_KEY_UP ) == GLFW_PRESS){
            camera.position += camera_direction * dt * camera_movement_speed;
        }
        // Move backward
        if (glfwGetKey( window, GLFW_KEY_DOWN ) == GLFW_PRESS){
            camera.position -= camera_direction * dt * camera_movement_speed;
        }
        // Strafe right
        if (glfwGetKey( window, GLFW_KEY_RIGHT ) == GLFW_PRESS){
            camera.position += camera_right * dt * camera_movement_speed;
        }
        // Strafe left
        if (glfwGetKey( window, GLFW_KEY_LEFT ) == GLFW_PRESS){
            camera.position -= camera_right * dt * camera_movement_speed;
        }

        camera.target = camera.position + camera_direction_rotated;
        updateCameraView(camera);

        glfwSetCursorPos(window, (float)window_width/2, (float)window_height/2);
        glfwGetCursorPos(window, &mouse_position.x, &mouse_position.y);
    }

    space_state = glfwGetKey(window, GLFW_KEY_SPACE);
    if(mouse_left_state == GLFW_RELEASE && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) mouse_left_press_time = glfwGetTime();
    mouse_left_state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
}
