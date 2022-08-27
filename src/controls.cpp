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
    bool right_mouse_click_press;
    bool right_mouse_click_release;

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

Entity* pickEntityWithMouse(Camera &camera, EntityManager &entity_manager) {
    glm::vec3 out_origin;
    glm::vec3 out_direction;
    screenPosToWorldRay(mouse_position, camera.view, camera.projection, out_origin, out_direction);

    Entity* sel_e = nullptr;
    float min_collision_distance = std::numeric_limits<float>::max();

    // Collision with bounded plane
    if (entity_manager.water != nullptr) {
        auto w_e = entity_manager.water;
        float t;
        if (lineIntersectsPlane(w_e->position, glm::vec3(0, 1, 0), out_origin, out_direction, t)) {
            auto p = glm::abs((out_origin + out_direction * t) - w_e->position);
            if (p.x <= w_e->scale[0][0] && p.z <= w_e->scale[2][2]) {
                auto collision_distance = glm::length((out_origin + out_direction * t) - camera.position);
                if (collision_distance < min_collision_distance) {
                    min_collision_distance = collision_distance;
                    sel_e = w_e;
                    camera.target = w_e->position;
                }
            }
        }
    }
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        auto m_e = (MeshEntity*)entity_manager.entities[i];
        if (m_e == nullptr || m_e->type != EntityType::MESH_ENTITY || m_e->mesh == nullptr) continue;

        const auto& mesh = m_e->mesh;
        const auto trans = createModelMatrix(m_e->position, m_e->rotation, m_e->scale);
        for (int j = 0; j <= mesh->num_indices - 3; j += 3) {
            if (mesh->indices[j  ] >= mesh->num_vertices || 
                mesh->indices[j+1] >= mesh->num_vertices || 
                mesh->indices[j+2] >= mesh->num_vertices) 
                continue;
            const auto p1 = mesh->vertices[mesh->indices[j]];
            const auto p2 = mesh->vertices[mesh->indices[j + 1]];
            const auto p3 = mesh->vertices[mesh->indices[j + 2]];
            glm::vec3 triangle[3] = {
                glm::vec3(trans * glm::vec4(p1, 1.0)),
                glm::vec3(trans * glm::vec4(p2, 1.0)),
                glm::vec3(trans * glm::vec4(p3, 1.0))
            };
            double u, v, t;
            if (rayIntersectsTriangle(triangle, out_origin, out_direction, t, u, v)) {
                auto collision_distance = glm::length((out_origin + out_direction * (float)t) - camera.position);
                if (collision_distance < min_collision_distance) {
                    min_collision_distance = collision_distance;
                    sel_e = m_e;
                    camera.target = m_e->position;
                }
            }
        }
    }
    return sel_e;
}

BoxCollider* pickColliderWithMouse(Camera& camera, EntityManager& entity_manager, glm::vec3 &normal) {
    glm::vec3 out_origin;
    glm::vec3 out_direction;
    screenPosToWorldRay(mouse_position, camera.view, camera.projection, out_origin, out_direction);

    float min_collision_distance = std::numeric_limits<float>::max();
    std::cout << "Pick collider\n";

    float min_t;
    BoxCollider* nearest_collider = nullptr;
    for (auto &collider : entity_manager.colliders) {
        float t;
        glm::vec3 n;
        if (lineIntersectsCube(collider.position, glm::vec3(1.0), out_origin, out_direction, t, n)) {
            if (nearest_collider == nullptr || t < min_t) {
                min_t = t;
                normal = n;
                nearest_collider = &collider;
                std::cout << "Collided at t " << min_t << " with normal " << normal << "\n";
            }
        }
    }
    return nearest_collider;
}

void handleEditorControls(Camera &editor_camera, Camera &level_camera, EntityManager &entity_manager, AssetManager &asset_manager, float dt) {
    // Stores the previous state of input, updated at end of function
    static int c_key_state = GLFW_RELEASE;
    static int p_key_state = GLFW_RELEASE;
    static int d_key_state = GLFW_RELEASE;
    static int f_key_state = GLFW_RELEASE;
    static int b_key_state = GLFW_RELEASE;
    static int delete_key_state = GLFW_RELEASE;
    static int mouse_left_state = GLFW_RELEASE;
    static int mouse_right_state = GLFW_RELEASE;
    static int ctrl_v_state = GLFW_RELEASE;
    static int backtick_key_state = GLFW_RELEASE;
    static double mouse_left_press_time = glfwGetTime();
    static glm::vec3 shooter_camera_velocity = glm::vec3(0.0);
    static float shooter_camera_deceleration = 0.8;

    static Id copy_id = NULLID;

    bool camera_movement_active = !editor::transform_active;
    ImGuiIO& io = ImGui::GetIO();
    controls::left_mouse_click_press   = !io.WantCaptureMouse && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT ) == GLFW_PRESS   && mouse_left_state == GLFW_RELEASE;
    controls::left_mouse_click_release = !io.WantCaptureMouse && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT ) == GLFW_RELEASE && mouse_left_state == GLFW_PRESS;
    controls::right_mouse_click_press  = !io.WantCaptureMouse && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS   && mouse_right_state == GLFW_RELEASE;
    controls::right_mouse_click_release= !io.WantCaptureMouse && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_RELEASE && mouse_right_state == GLFW_PRESS;

    // Unlike other inputs, calculate delta but update mouse position immediately
    glm::dvec2 delta_mouse_position = mouse_position;
    glfwGetCursorPos(window, &mouse_position.x, &mouse_position.y);
    delta_mouse_position = mouse_position - delta_mouse_position;

    if(backtick_key_state == GLFW_RELEASE && glfwGetKey(window, GLFW_KEY_GRAVE_ACCENT) == GLFW_PRESS) 
        editor::do_terminal = !editor::do_terminal;
    
    if (!io.WantCaptureKeyboard && glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS && c_key_state == GLFW_RELEASE
        && glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) != GLFW_PRESS && glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) != GLFW_PRESS)
        editor::use_level_camera = !editor::use_level_camera;
    
    if (!io.WantCaptureKeyboard && glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS && b_key_state == GLFW_RELEASE) {
        editor::editor_mode = (editor::EditorMode)(((int)editor::editor_mode + 1) % (int)editor::EditorMode::NUM);
        editor::selection = editor::Selection();
    }

    Entity* sel_e;
    if (editor::selection.is_water)
        sel_e = entity_manager.water;
    else
        sel_e = entity_manager.getEntity(editor::selection.id);

    Camera* camera_ptr = &editor_camera;
    if (editor::use_level_camera)
        camera_ptr = &level_camera;
    Camera& camera = *camera_ptr;

    if(!io.WantCaptureKeyboard){
        if (sel_e != nullptr && glfwGetKey(window, GLFW_KEY_DELETE) == GLFW_PRESS && delete_key_state == GLFW_RELEASE) {
            if (sel_e->type == WATER_ENTITY) {
                free(entity_manager.water);
                entity_manager.water = nullptr;
            }
            else {
                entity_manager.deleteEntity(sel_e->id);
            }
            sel_e = nullptr;
            editor::selection = editor::Selection();
        }
        if (sel_e != nullptr && sel_e->type != WATER_ENTITY && glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS && c_key_state == GLFW_RELEASE && glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
            copy_id = sel_e->id;
        }
        if (copy_id.i != -1 && ctrl_v_state == GLFW_RELEASE && glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS && glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS && entity_manager.getEntity(copy_id) != nullptr) {
            auto e = entity_manager.duplicateEntity(copy_id);
            copy_id = e->id;

            editor::selection.id = copy_id;
            editor::selection.is_water = e->type == WATER_ENTITY;
            if (e->type == MESH_ENTITY) {
                ((MeshEntity*)e)->position.x += editor::translation_snap.x;
                if (camera.state == Camera::TYPE::TRACKBALL) {
                    camera.target = ((MeshEntity*)e)->position;
                    updateCameraView(camera);
                }
            }
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS && d_key_state == GLFW_RELEASE && glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
            editor::draw_debug_wireframe = !editor::draw_debug_wireframe;
        }
        if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS && c_key_state == GLFW_RELEASE
            && glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) != GLFW_PRESS && glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
            if (camera.state == Camera::TYPE::TRACKBALL) {
                camera.state = Camera::TYPE::STATIC;
            }
            else if (camera.state == Camera::TYPE::SHOOTER) {
                camera.state = Camera::TYPE::TRACKBALL;

                if (sel_e != nullptr) {
                    auto s_m_e = reinterpret_cast<MeshEntity*>(sel_e);
                    if (s_m_e != nullptr && s_m_e->type == EntityType::MESH_ENTITY) {
                        camera.target = s_m_e->position;
                        updateCameraView(camera);
                        updateShadowVP(camera);
                    }
                }
            }
            else if (camera.state == Camera::TYPE::STATIC) {
                camera.state = Camera::TYPE::SHOOTER;

                mouse_position = glm::dvec2(window_width / 2, window_height / 2);
                delta_mouse_position = glm::dvec2(0, 0);
            }
        }

        if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS && p_key_state == GLFW_RELEASE)
            playing = !playing;

        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS && f_key_state == GLFW_RELEASE)
            editor::draw_level_camera = !editor::draw_level_camera;
    }

    if (right_mouse_click_release) {
        if (editor::editor_mode == editor::EditorMode::COLLIDERS) {
            auto collider = pickColliderWithMouse(camera, entity_manager, glm::vec3());
            if (collider != nullptr) {
                entity_manager.deleteEntity(collider->linked_id);
                entity_manager.removeBoxCollider(collider);
            }
        }
        else {
            sel_e = nullptr;
            editor::selection = editor::Selection();
        }
    }
    if (left_mouse_click_release && (glfwGetTime() - mouse_left_press_time) < 0.2) {
        switch (editor::editor_mode)
        {
        case editor::EditorMode::ENTITY:
        {
            sel_e = pickEntityWithMouse(camera, entity_manager);
            if (sel_e != nullptr) {
                editor::selection.id = sel_e->id;
                editor::selection.is_water = sel_e->type == WATER_ENTITY;
                if (camera.state != Camera::TYPE::STATIC) {
                    updateCameraView(camera);
                    updateShadowVP(camera);
                }
            }
            else {
                editor::selection = editor::Selection();
            }
            break;
        }
        case editor::EditorMode::COLLIDERS:
        {
            if (entity_manager.colliders.size() == 0) {
                auto cube = new MeshEntity(entity_manager.getFreeId());
                auto cube_mesh = asset_manager.getMesh("data/mesh/cube.mesh");
                if (cube_mesh == nullptr) {
                    cube->mesh = asset_manager.createMesh("data/mesh/cube.mesh");
                    asset_manager.loadMesh(cube->mesh, "data/mesh/cube.mesh", true);
                }
                else {
                    cube->mesh = cube_mesh;
                }
                entity_manager.setEntity(cube->id.i, cube);
                entity_manager.addBoxCollider(cube->position, cube->id);
            }
            else {
                glm::vec3 normal;
                auto collider = pickColliderWithMouse(camera, entity_manager, normal);
                if (collider != nullptr) {
                    std::cout << "Collided, normal: " << normal << "\n";
                    auto cube = new MeshEntity(entity_manager.getFreeId());
                    auto cube_mesh = asset_manager.getMesh("data/mesh/cube.mesh");
                    if (cube_mesh == nullptr) {
                        cube->mesh = asset_manager.createMesh("data/mesh/cube.mesh");
                        asset_manager.loadMesh(cube->mesh, "data/mesh/cube.mesh", true);
                    }
                    else {
                        cube->mesh = cube_mesh;
                    }
                    cube->position = collider->position + 2.0f * normal;
                    entity_manager.setEntity(cube->id.i, cube);
                    entity_manager.addBoxCollider(cube->position, cube->id);
                }
            }
            break;
        }
        default:
        {
            break;
        }
        }
        
    }

    if(camera.state == Camera::TYPE::TRACKBALL){    
        if (!io.WantCaptureMouse && !(left_mouse_click_release && (glfwGetTime() - mouse_left_press_time) < 0.2) && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS && camera_movement_active) {
            // Calculate the amount of rotation given the mouse movement.
            float delta_angle_x = (2 * PI / (float)window_width); // a movement from left to right = 2*PI = 360 deg
            float delta_angle_y = (PI / (float)window_height);  // a movement from top to bottom = PI = 180 deg
            float x_angle = -delta_mouse_position.x * delta_angle_x;
            float y_angle = -delta_mouse_position.y * delta_angle_y;

            auto camera_look = camera.position - camera.target;

            //printf("Camera look: x %f y %f z %f.\n", camera_look.x, camera_look.y, camera_look.z);
            //printf("Camera right: x %f y %f z %f.\n", camera.right.x, camera.right.y, camera.right.z);

            // Rotate the camera around the pivot point on the first axis.
            //glm::quat rotation_x;
            //if(glm::abs(glm::dot(camera_look, camera.right)) < 0.0001) rotation_x = glm::angleAxis(x_angle, camera.right);
            //else                                                       rotation_x = glm::angleAxis(x_angle, camera.up);
            
            auto rotation_x = glm::angleAxis(x_angle, camera.up);
            camera_look = rotation_x * camera_look;

            // Handle camera passing over poles of orbit 
            // cos of angle between look and up is close to 1 -> parallel, -1 -> antiparallel
            auto l_cos_up = glm::dot(camera_look, camera.up) / glm::length(camera_look);
            bool allow_rotation = true;
            if(abs(1 - l_cos_up) <= 0.01) {
                allow_rotation = y_angle > 0.f;
            } else if (abs(l_cos_up + 1) <= 0.01) {
                allow_rotation = y_angle < 0.f;
            }
            if (allow_rotation){
                auto rotation_y = glm::angleAxis(y_angle, camera.right);
                camera_look = rotation_y * camera_look;
            }

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
        static const float camera_movement_acceleration = 2.0;

        glm::vec3 camera_direction_rotated;
        if(delta_mouse_position.length() != 0){
            camera.forward = glm::normalize(camera.target - camera.position);
            // Calculate the amount of rotation given the mouse movement.
            float delta_angle_x = (2 * PI / (float)window_width); // a movement from left to right = 2*PI = 360 deg
            float delta_angle_y = (PI / (float)window_height);  // a movement from top to bottom = PI = 180 deg
            float x_angle = -delta_mouse_position.x * delta_angle_x;
            float y_angle = -delta_mouse_position.y * delta_angle_y;

            // @todo Handle camera passing over poles of orbit 
            
            auto rotation_x = glm::rotate(glm::mat4x4(1.0f), x_angle, camera.up);
            auto rotation_y = glm::rotate(glm::mat4x4(1.0f), y_angle, camera.right);

            camera_direction_rotated = glm::vec3(rotation_y * (rotation_x * glm::vec4(camera.forward, 1)));
        }

        if (!io.WantCaptureKeyboard) {
            if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) != GLFW_PRESS) {
                if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
                    shooter_camera_velocity += camera.forward * camera_movement_acceleration;
                }
                if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
                    shooter_camera_velocity -= camera.forward * camera_movement_acceleration;
                }
                if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                    shooter_camera_velocity += camera.right * camera_movement_acceleration;
                }
                if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                    shooter_camera_velocity -= camera.right * camera_movement_acceleration;
                }
            }
            if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
                if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
                    shooter_camera_velocity -= camera.up * camera_movement_acceleration;
                }
                else {
                    shooter_camera_velocity += camera.up * camera_movement_acceleration;
                }
            }
        }
        camera.position += shooter_camera_velocity*dt;
        shooter_camera_velocity *= shooter_camera_deceleration;

        auto new_target = camera.position + camera_direction_rotated;
        if(new_target != camera.target){
            camera.target = new_target;
            updateCameraView(camera);
            updateShadowVP(camera);
        }

        glfwSetCursorPos(window, (float)window_width/2, (float)window_height/2);
        glfwGetCursorPos(window, &mouse_position.x, &mouse_position.y);
    }

    ctrl_v_state        = glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS && glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
    backtick_key_state  = glfwGetKey(window, GLFW_KEY_GRAVE_ACCENT);
    delete_key_state    = glfwGetKey(window, GLFW_KEY_DELETE);
    c_key_state         = glfwGetKey(window, GLFW_KEY_C);
    d_key_state         = glfwGetKey(window, GLFW_KEY_D);
    f_key_state         = glfwGetKey(window, GLFW_KEY_F);
    p_key_state         = glfwGetKey(window, GLFW_KEY_P);
    b_key_state         = glfwGetKey(window, GLFW_KEY_B);

    if(mouse_left_state == GLFW_RELEASE && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) mouse_left_press_time = glfwGetTime();
    mouse_left_state    = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
    mouse_right_state   = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
}

void handleGameControls() {
    static int p_key_state = GLFW_PRESS;

    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS && p_key_state == GLFW_RELEASE) {
        playing = !playing;
    }

    p_key_state = glfwGetKey(window, GLFW_KEY_P);
}