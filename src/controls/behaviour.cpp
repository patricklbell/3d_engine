#include <iostream>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui.h>

#include <controls/behaviour.hpp>
#include <controls/globals.hpp>

#include <camera/globals.hpp>

#include "utilities.hpp"
#include "entities.hpp"
#include "editor.hpp"
#include "game_behaviour.hpp"

Entity* pickEntityWithMouse(Camera& camera, EntityManager& entity_manager) {
    glm::vec3 out_origin;
    glm::vec3 out_direction;
    screenPosToWorldRay(Controls::mouse_position, camera.view, camera.projection, out_origin, out_direction);

    Entity* closest_e = nullptr;
    float min_collision_distance = std::numeric_limits<float>::max();

    for (int i = 0; i < ENTITY_COUNT; ++i) {
        auto e = entity_manager.entities[i];
        if (e == nullptr) continue;

        // Collision with bounded plane
        if (e->type & EntityType::WATER_ENTITY) {
            auto w_e = (WaterEntity*)e;
            float t;
            if (lineIntersectsPlane(w_e->position, glm::vec3(0, 1, 0), out_origin, out_direction, t)) {
                auto p = glm::abs((out_origin + out_direction * t) - w_e->position);
                if (p.x <= w_e->scale[0][0] && p.z <= w_e->scale[2][2]) {
                    auto collision_distance = glm::length((out_origin + out_direction * t) - camera.position);
                    if (collision_distance < min_collision_distance) {
                        min_collision_distance = collision_distance;
                        closest_e = w_e;
                        camera.set_target(w_e->position);
                    }
                }
            }
        }
        else if (e->type & EntityType::MESH_ENTITY && ((MeshEntity*)e)->mesh != nullptr) {
            auto m_e = (MeshEntity*)e;
            const auto& mesh = m_e->mesh;
            const auto g_trans = createModelMatrix(m_e->position, m_e->rotation, m_e->scale);

            for (int j = 0; j <= (int64_t)mesh->num_indices - 3; j += 3) {
                if (mesh->indices[j] >= mesh->num_vertices ||
                    mesh->indices[j + 1] >= mesh->num_vertices ||
                    mesh->indices[j + 2] >= mesh->num_vertices)
                    continue;
                const auto p1 = mesh->vertices[mesh->indices[j]];
                const auto p2 = mesh->vertices[mesh->indices[j + 1]];
                const auto p3 = mesh->vertices[mesh->indices[j + 2]];
                for (int k = 0; k < mesh->num_meshes; k++) {
                    auto trans = mesh->transforms[k] * g_trans;
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
                            closest_e = e;
                            camera.set_target(m_e->position);
                        }
                    }
                }
            }
        }
    }

    return closest_e;
}

ColliderEntity* pickColliderWithMouse(Camera& camera, EntityManager& entity_manager, glm::vec3& normal, bool only_selectable = false) {
    glm::vec3 out_origin;
    glm::vec3 out_direction;
    screenPosToWorldRay(Controls::mouse_position, camera.view, camera.projection, out_origin, out_direction);

    float min_collision_distance = std::numeric_limits<float>::max();
    std::cout << "Pick collider\n";

    float min_t;
    ColliderEntity* nearest_c = nullptr;
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        auto c = (ColliderEntity*)entity_manager.entities[i];
        if (c == nullptr || !entityInherits(c->type, COLLIDER_ENTITY)) continue;
        if (only_selectable && !c->selectable) continue;
        std::cout << "testing collider entity\n";

        float t;
        glm::vec3 n;
        auto scl = glm::vec3(c->collider_scale[0][0], c->collider_scale[1][1], c->collider_scale[2][2]);
        if (lineIntersectsCube(c->collider_position, scl, out_origin, out_direction, t, n)) {
            if (nearest_c == nullptr || t < min_t) {
                min_t = t;
                normal = n;
                nearest_c = c;
                std::cout << "Collided at t " << min_t << " with normal " << normal << "\n";
            }
        }
    }
    return nearest_c;
}

void handleEditorControls(EntityManager*& entity_manager, AssetManager& asset_manager, float dt) {
    ImGuiIO& io = ImGui::GetIO();
    static float mouse_hold_threshold = 0.1; // The time past which a mouse press is considered a hold

    if (Controls::editor.isAction("toggle_terminal"))
        editor::do_terminal = !editor::do_terminal;

    // @todo
    Camera* camera_ptr = &Cameras::editor_camera;
    if (editor::use_level_camera)
        camera_ptr = &Cameras::level_camera;
    Camera& camera = *camera_ptr;

    if (!io.WantCaptureKeyboard) {
        if (Controls::editor.isAction("delete_selection") && editor::selection.ids.size()) {
            for (auto& id : editor::selection.ids) {
                entity_manager->deleteEntity(id);
                if (id == entity_manager->water) {
                    entity_manager->water = NULLID;
                }
            }
            editor::selection.clear();
        }
        if (Controls::editor.isAction("toggle_collider_visibility"))
            editor::draw_colliders = !editor::draw_colliders;

        if (Controls::editor.isAction("switch_cameras"))
            editor::use_level_camera = !editor::use_level_camera;

        if (Controls::editor.isAction("switch_editor_mode")) {
            editor::editor_mode = (EditorMode)(((int)editor::editor_mode + 1) % (int)EditorMode::NUM);
            editor::selection.clear();
        }

        if (Controls::editor.isAction("copy") &&
            editor::selection.ids.size() && !(editor::selection.type & WATER_ENTITY)) {
            referenceToCopySelection(*entity_manager, editor::selection, editor::copy_selection);
            pushInfoMessage("Copied selection");
        }

        if (Controls::editor.isAction("paste") && editor::copy_selection.entities.size() != 0) {
            editor::selection.clear();
            createCopySelectionEntities(*entity_manager, asset_manager, editor::copy_selection, editor::selection);
            camera.set_target(editor::selection.avg_position);
        }

        if (Controls::editor.isAction("toggle_debug_visibility")) {
            editor::draw_debug_wireframe = !editor::draw_debug_wireframe;
            pushInfoMessage(editor::draw_debug_wireframe ? "Debug wireframe on" : "Debug wireframe off");
        }

        if (Controls::editor.isAction("switch_camera_mode")) {
            if (camera.state == Camera::TYPE::TRACKBALL) {
                camera.state = Camera::TYPE::STATIC;
                pushInfoMessage("Static camera");
            }
            else if (camera.state == Camera::TYPE::SHOOTER) {
                camera.state = Camera::TYPE::TRACKBALL;
                if (editor::selection.ids.size() == 1) {
                    camera.set_target(editor::selection.avg_position);
                }
                pushInfoMessage("Orbit camera");
            }
            else if (camera.state == Camera::TYPE::STATIC) {
                camera.state = Camera::TYPE::SHOOTER;

                Controls::mouse_position = glm::dvec2(window_width / 2, window_height / 2);
                Controls::delta_mouse_position = glm::dvec2(0, 0);
                pushInfoMessage("Shooter style camera");
            }
        }

        if (Controls::editor.isAction("toggle_playing")) {
            playGame(entity_manager);
            pushInfoMessage("Playing");
        }

        if (Controls::editor.isAction("toggle_level_camera_visibility")) {
            editor::draw_level_camera = !editor::draw_level_camera;
            pushInfoMessage(editor::draw_level_camera ? "Camera wireframe on" : "Camera wireframe off");
        }
    }

    if (!io.WantCaptureMouse) {
        if (Controls::editor.right_mouse.state == Controls::RELEASE && (glfwGetTime() - Controls::editor.right_mouse.last_press) < mouse_hold_threshold) {
            if (editor::editor_mode == EditorMode::COLLIDERS) {
                glm::vec3 n;
                auto collider = pickColliderWithMouse(camera, *entity_manager, n);
                if (collider != nullptr) {
                    entity_manager->deleteEntity(collider->id);
                }
            }
            else {
                editor::selection.clear();
            }
        }
        if (Controls::editor.left_mouse.state == Controls::RELEASE && (glfwGetTime() - Controls::editor.left_mouse.last_press) < mouse_hold_threshold) {
            switch (editor::editor_mode)
            {
            case EditorMode::ENTITY:
            {
                auto e = pickEntityWithMouse(camera, *entity_manager);
                if (e != nullptr) {
                    if (!glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)) {
                        editor::selection.clear();
                    }
                    editor::selection.toggleEntity(*entity_manager, e);

                    if (camera.state != Camera::TYPE::STATIC && editor::selection.avg_position_count) {
                        camera.set_target(editor::selection.avg_position);
                    }
                }
                else {
                    editor::selection.clear();
                }
                break;
            }
            case EditorMode::COLLIDERS:
            {

                glm::vec3 normal;
                auto pick_c = pickColliderWithMouse(camera, *entity_manager, normal);
                if (pick_c != nullptr) {
                    std::cout << "Collided, normal: " << normal << "\n";
                    auto c = (ColliderEntity*)copyEntity((Entity*)pick_c);
                    c->id = entity_manager->getFreeId();
                    c->mesh = pick_c->mesh;

                    c->position = pick_c->position + normal;
                    c->scale = pick_c->scale;
                    c->rotation = pick_c->rotation;
                    c->collider_position = pick_c->collider_position + normal;
                    c->collider_scale = pick_c->collider_scale;
                    c->collider_rotation = pick_c->collider_rotation;

                    entity_manager->setEntity(c->id.i, (Entity*)c);
                }
                break;
            }
            default:
            {
                break;
            }
            }
        }
    }

    bool camera_movement_active = !editor::transform_active && !io.WantCaptureMouse && !io.WantCaptureKeyboard;
    if (camera.state == Camera::TYPE::TRACKBALL && camera_movement_active) {
        if (Controls::editor.left_mouse.state == Controls::HELD && (glfwGetTime() - Controls::editor.left_mouse.last_press) > mouse_hold_threshold) {
            // Calculate the amount of rotation given the mouse movement.
            float delta_angle_x = (2 * PI / (float)window_width); // a movement from left to right = 2*PI = 360 deg
            float delta_angle_y = (PI / (float)window_height);  // a movement from top to bottom = PI = 180 deg
            float x_angle = -Controls::delta_mouse_position.x * delta_angle_x;
            float y_angle = -Controls::delta_mouse_position.y * delta_angle_y;

            auto camera_look = camera.position - camera.target;

            auto rotation_x = glm::angleAxis(x_angle, camera.up);
            camera_look = rotation_x * camera_look;

            // Handle camera passing over poles of orbit 
            // cos of angle between look and up is close to 1 -> parallel, -1 -> antiparallel
            auto l_cos_up = glm::dot(camera_look, camera.up) / glm::length(camera_look);
            bool allow_rotation = true;
            if (abs(1 - l_cos_up) <= 0.01) {
                allow_rotation = y_angle > 0.f;
            }
            else if (abs(l_cos_up + 1) <= 0.01) {
                allow_rotation = y_angle < 0.f;
            }
            if (allow_rotation) {
                auto rotation_y = glm::angleAxis(y_angle, camera.right);
                camera_look = rotation_y * camera_look;
            }

            // Update the camera view
            camera.set_position(camera_look + camera.target);
        }
        else if (Controls::editor.right_mouse.state == Controls::HELD && (glfwGetTime() - Controls::editor.right_mouse.last_press) > mouse_hold_threshold) {
            auto inverse_vp = glm::inverse(camera.projection * camera.view);

            // Find Normalized Device coordinates mouse positions
            auto new_mouse_position_ndc = (Controls::mouse_position / glm::dvec2((float)window_width, (float)window_height) - glm::dvec2(0.5)) * 2.0;
            auto old_mouse_position = Controls::mouse_position - Controls::delta_mouse_position;
            auto old_mouse_position_ndc = (old_mouse_position / glm::dvec2((float)window_width, (float)window_height) - glm::dvec2(0.5)) * 2.0;

            // Project these mouse coordinates onto near plane to determine world coordinates
            auto new_mouse_position_world = glm::vec3(inverse_vp * glm::vec4(new_mouse_position_ndc.x, -new_mouse_position_ndc.y, 0, 1));
            auto old_mouse_position_world = glm::vec3(inverse_vp * glm::vec4(old_mouse_position_ndc.x, -old_mouse_position_ndc.y, 0, 1));

            // Scale movement such that point under mouse on plane of target (parallel to near plane) stays constant
            float ratio;
            ratio = glm::length(camera.position - camera.target);
            auto delta = ratio * (new_mouse_position_world - old_mouse_position_world);

            camera.set_position(camera.position - delta);
            camera.set_target(camera.target - delta);
        }
        if (Controls::scroll_offset.y != 0) {
            float distance = glm::length(camera.position - camera.target);
            distance = abs(distance + distance * Controls::scroll_offset.y * 0.1);

            camera.set_position(camera.target + glm::normalize(camera.position - camera.target) * distance);
        }
    }
    else if (camera.state == Camera::TYPE::SHOOTER && camera_movement_active) {
        static glm::vec3 camera_velocity = glm::vec3(0.0);
        static const float camera_resistance = 50.0; // The "air resistance" the camera experiences
        static float camera_acceleration = 500.0;

        // Change acceleration by scrolling
        if (Controls::scroll_offset.y != 0) {
            camera_acceleration = abs(camera_acceleration + camera_acceleration * Controls::scroll_offset.y * 0.1);
            camera_acceleration = glm::clamp(camera_acceleration, 200.0f, 1000.0f);
            pushInfoMessage("Camera acceleration " + std::to_string(camera_acceleration), InfoMessage::Urgency::NORMAL, 1.0, "#acceleration");
        }

        glm::vec3 camera_direction_rotated;
        if (Controls::delta_mouse_position.length() != 0) {
            camera.forward = glm::normalize(camera.target - camera.position);
            // Calculate the amount of rotation given the mouse movement.
            float delta_angle_x = (2 * PI / (float)window_width); // a movement from left to right = 2*PI = 360 deg
            float delta_angle_y = (PI / (float)window_height);  // a movement from top to bottom = PI = 180 deg
            float x_angle = -Controls::delta_mouse_position.x * delta_angle_x;
            float y_angle = -Controls::delta_mouse_position.y * delta_angle_y;

            // @todo Handle camera passing over poles of orbit 

            auto rotation_x = glm::rotate(glm::mat4x4(1.0f), x_angle, camera.up);
            auto rotation_y = glm::rotate(glm::mat4x4(1.0f), y_angle, camera.right);

            camera_direction_rotated = glm::vec3(rotation_y * (rotation_x * glm::vec4(camera.forward, 1)));
        }

        if (Controls::editor.isAction("fly_forward")) {
            camera_velocity += camera.forward * camera_acceleration * dt;
        }
        if (Controls::editor.isAction("fly_backwards")) {
            camera_velocity -= camera.forward * camera_acceleration * dt;
        }
        if (Controls::editor.isAction("fly_right")) {
            camera_velocity += camera.right * camera_acceleration * dt;
        }
        if (Controls::editor.isAction("fly_left")) {
            camera_velocity -= camera.right * camera_acceleration * dt;
        }
        if (Controls::editor.isAction("fly_up")) {
            camera_velocity += camera.up * camera_acceleration * dt;
        }
        if (Controls::editor.isAction("fly_down")) {
            camera_velocity -= camera.up * camera_acceleration * dt;
        }
        camera.set_position(camera.position + camera_velocity * dt);

        auto previous_camera_velocity_direction = glm::normalize(camera_velocity);
        camera_velocity -= camera_resistance*camera_velocity*dt;
        // Ensure resistance doesn't overshoot
        if (glm::dot(camera_velocity, previous_camera_velocity_direction) < 0)
            camera_velocity = glm::vec3(0.0);

        auto new_target = camera.position + camera_direction_rotated;
        if (new_target != camera.target) {
            camera.set_target(new_target);
        }

        glfwSetCursorPos(window, (float)window_width / 2, (float)window_height / 2);
        Controls::mouse_position = glm::dvec2(window_width / 2, window_height / 2);
        Controls::delta_mouse_position = glm::dvec2(0, 0);
    }
}

void handleGameControls(EntityManager*& entity_manager, AssetManager& asset_manager, float dt) {
    if (Controls::editor.isAction("toggle_terminal"))
        editor::do_terminal = !editor::do_terminal;

    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureKeyboard) {
        if (Controls::editor.isAction("toggle_playing")) {
            pauseGame(entity_manager);
            pushInfoMessage("Switched to editor");
        }
        if (Controls::game.isAction("reset")) {
            resetGameEntities();
            pushInfoMessage("Resetting");
        }
        if (Controls::game.isAction("toggle_animation_state_visibility"))
            editor::debug_animations = !editor::debug_animations;

        if (Controls::game.isAction("slow_time")) {
            global_time_warp *= 0.8;
            pushInfoMessage("Time warp " + std::to_string(global_time_warp), InfoMessage::Urgency::NORMAL, 0.25, "#time-warp");
        }
        if (Controls::game.isAction("fast_time")) {
            global_time_warp *= 1.2;
            pushInfoMessage("Time warp " + std::to_string(global_time_warp), InfoMessage::Urgency::NORMAL, 0.25, "#time-warp");
        }

        if (Controls::game.isAction("pause"))
            global_paused = !global_paused;

        // Handle player controls
        if (entity_manager->player != NULLID) {
            auto player = (PlayerEntity*)entity_manager->getEntity(entity_manager->player);
            if (player != nullptr) {
                if (Controls::game.isAction("move_left")) {
                    if (player->turn_left()) {
                        if (!player->step_forward()) {
                            player->actions.pop_back();
                        }
                    }
                }
                if (Controls::game.isAction("move_right")) {
                    if (player->turn_right()) {
                        if (!player->step_forward()) {
                            player->actions.pop_back();
                        }
                    }
                }
                if (Controls::game.isAction("move_forward")) {
                    player->step_forward();
                }
                if (Controls::game.isAction("turn_around")) {
                    if (player->turn_right()) {
                        if (!player->turn_right()) {
                            player->actions.pop_back();
                        }
                    }
                }
            }
        }
    }
}