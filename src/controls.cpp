#include <iostream>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Include ImGui
#include "entities.hpp"
#include "glm/detail/func_geometric.hpp"
#include "glm/gtc/quaternion.hpp"
#include "imgui.h"

#include "controls.hpp"
#include "editor.hpp"
#include "graphics.hpp"
#include "utilities.hpp"
#include "globals.hpp"


namespace controls {
    glm::vec2 scroll_offset;
    bool scrolled;
    bool left_mouse_click_press;
    bool left_mouse_click_release;

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

void handleEditorControls(Camera &camera, EntityManager &entity_manager, float dt) {
    // Stores the previous state of input, updated at end of function
    static int space_key_state = GLFW_RELEASE;
    static int d_key_state = GLFW_RELEASE;
    static int mouse_left_state = GLFW_RELEASE;
    static double mouse_left_press_time = glfwGetTime();

    bool camera_movement_active = !editor::transform_active;
    ImGuiIO& io = ImGui::GetIO();
    controls::left_mouse_click_press   = !io.WantCaptureMouse && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS && mouse_left_state == GLFW_RELEASE;
    controls::left_mouse_click_release = !io.WantCaptureMouse && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE && mouse_left_state == GLFW_PRESS;

    // Unlike other inputs, calculate delta but update mouse position immediately
    glm::dvec2 delta_mouse_position = mouse_position;
    glfwGetCursorPos(window, &mouse_position.x, &mouse_position.y);
    delta_mouse_position = mouse_position - delta_mouse_position;

    if(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS && d_key_state == GLFW_RELEASE){
        //editor::draw_bt_debug = !editor::draw_bt_debug; 
        editor::draw_debug_wireframe = !editor::draw_debug_wireframe;
    }
    if(glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && space_key_state == GLFW_RELEASE){
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
                auto s_m_e = (MeshEntity*)entity_manager.getEntity(selected_entity);
                if(s_m_e != nullptr && s_m_e->type == EntityType::MESH_ENTITY){
                    camera.target = s_m_e->position;
                    updateCameraView(camera);
                    updateShadowVP(camera);
                }
            }
        }
    }


    if(camera.state == Camera::TYPE::TRACKBALL){
        if (left_mouse_click_release && (glfwGetTime() - mouse_left_press_time) < 0.2) {
            glm::vec3 out_origin;
            glm::vec3 out_direction;
            screenPosToWorldRay(mouse_position, camera.view, camera.projection, out_origin, out_direction);
            
            selected_entity = -1;
            float min_collision_distance = std::numeric_limits<float>::max();
            for(int i = 0; i < ENTITY_COUNT; ++i){
                auto m_e = (MeshEntity*)entity_manager.getEntity(i);
                if(m_e == nullptr || !(m_e->type & EntityType::MESH_ENTITY)) continue;

                const auto mesh = m_e->mesh;
                const auto trans = createModelMatrix(m_e->position, m_e->rotation, m_e->scale);
                for(int j = 0; j < mesh->num_indices; j+=3){
                    const auto p1 = mesh->vertices[mesh->indices[j]];
                    const auto p2 = mesh->vertices[mesh->indices[j+1]];
                    const auto p3 = mesh->vertices[mesh->indices[j+2]];
                    glm::vec3 triangle[3] = {
                        glm::vec3(trans * glm::vec4(p1, 1.0)),
                        glm::vec3(trans * glm::vec4(p2, 1.0)),
                        glm::vec3(trans * glm::vec4(p3, 1.0))
                    };
                    double u, v, t;
                    if(rayIntersectsTriangle(triangle, out_origin, out_direction, t, u, v)){
                        auto collision_distance = glm::length((out_origin + out_direction*(float)t) - camera.position);
                        if(collision_distance < min_collision_distance){
                            min_collision_distance = collision_distance;
                            selected_entity = i;
                        }
                    }
                }
            }
            if(selected_entity != -1){
                camera.target = ((MeshEntity*)entity_manager.getEntity(selected_entity))->position;
                updateCameraView(camera);
                updateShadowVP(camera);
            }
        } else if (!io.WantCaptureMouse && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS && camera_movement_active) {
            auto camera_right = glm::vec3(glm::transpose(camera.view)[0]);

            // Calculate the amount of rotation given the mouse movement.
            float delta_angle_x = (2 * M_PI / (float)window_width); // a movement from left to right = 2*PI = 360 deg
            float delta_angle_y = (M_PI / (float)window_height);  // a movement from top to bottom = PI = 180 deg
            float x_angle = -delta_mouse_position.x * delta_angle_x;
            float y_angle = -delta_mouse_position.y * delta_angle_y;

            // @todo Handle camera passing over poles of orbit 
            auto camera_look = camera.position - camera.target;

            //printf("Camera look: x %f y %f z %f.\n", camera_look.x, camera_look.y, camera_look.z);
            //printf("Camera right: x %f y %f z %f.\n", camera_right.x, camera_right.y, camera_right.z);

            // Rotate the camera around the pivot point on the first axis.
            //glm::quat rotation_x;
            //if(glm::abs(glm::dot(camera_look, camera_right)) < 0.0001) rotation_x = glm::angleAxis(x_angle, camera_right);
            //else                                                       rotation_x = glm::angleAxis(x_angle, camera.up);
            
            auto rotation_x = glm::angleAxis(x_angle, camera.up);
            camera_look = rotation_x * camera_look;

            // Rotate the camera around the pivot point on the second axis.
            auto rotation_y = glm::angleAxis(y_angle, camera_right);
            camera_look = rotation_y * camera_look;

            // Update the camera view
            camera.position = camera_look + camera.target;
            updateCameraView(camera);
            updateShadowVP(camera);
        }
        if(!io.WantCaptureMouse && scroll_offset.y != 0){
            float distance = glm::length(camera.position - camera.target);
            distance = abs(distance + distance*scroll_offset.y*0.1);

            camera.position = camera.target + glm::normalize(camera.position - camera.target)*distance;
            updateCameraView(camera);
            updateShadowVP(camera);

            // Handle scroll event
            scroll_offset.y = 0;
        }
    } else if (camera.state == Camera::TYPE::SHOOTER && camera_movement_active){
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
        updateShadowVP(camera);

        glfwSetCursorPos(window, (float)window_width/2, (float)window_height/2);
        glfwGetCursorPos(window, &mouse_position.x, &mouse_position.y);
    }

    space_key_state = glfwGetKey(window, GLFW_KEY_SPACE);
    d_key_state     = glfwGetKey(window, GLFW_KEY_D);
    if(mouse_left_state == GLFW_RELEASE && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) mouse_left_press_time = glfwGetTime();
    mouse_left_state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
}
