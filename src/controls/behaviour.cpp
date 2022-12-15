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
#include <utilities/math.hpp>

#include "entities.hpp"
#include "editor.hpp"
#include "game_behaviour.hpp"

Raycast raycastEntities(Raycast& raycast, EntityManager& entity_manager, bool colliders=false) {
    auto tmp_raycast = raycast;
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        auto e = entity_manager.entities[i];
        if (e == nullptr) continue;

        tmp_raycast.result.hit = false;
        if (!colliders && e->type & EntityType::WATER_ENTITY) {
            auto w_e = (WaterEntity*)e;

            glm::vec3 bounds{ w_e->scale[0][0], w_e->scale[1][1], w_e->scale[2][2] };
            raycastBoundedPlane(w_e->position + bounds*glm::vec3(0.5,0,0.5), glm::vec3(0, 1, 0), bounds*0.5f, tmp_raycast);
        }
        else if (!colliders && e->type & EntityType::MESH_ENTITY && ((MeshEntity*)e)->mesh != nullptr) {
            const auto& m_e = (MeshEntity*)e;
            const auto& mesh = m_e->mesh;

            for (int j = 0; j < mesh->num_submeshes; j++) {
                auto model = createModelMatrix(mesh->transforms[j], m_e->position, m_e->rotation, 1.01f*m_e->scale);
                if (raycastTriangles(mesh->vertices, mesh->indices, mesh->num_indices, model, tmp_raycast)) {
                    if (tmp_raycast.result.hit && tmp_raycast.result.t < raycast.result.t) {
                        raycast = tmp_raycast;
                        raycast.result.indice = i;
                    }
                }
            }
            continue;
        }
        else if (colliders && (e->type & EntityType::COLLIDER_ENTITY)) {
            auto c = (ColliderEntity*)entity_manager.entities[i];
            auto bounds = glm::vec3(c->collider_scale[0][0], c->collider_scale[1][1], c->collider_scale[2][2]);
            raycastCube(c->collider_position + bounds/2.0f, bounds, tmp_raycast);
        }

        if (tmp_raycast.result.hit && tmp_raycast.result.t < raycast.result.t) {
            raycast = tmp_raycast;
            raycast.result.indice = i;
        }
    }

    return raycast;
}

Raycast raycastEntityWithMouse(const Camera& camera, EntityManager& entity_manager) {
    auto raycast = mouseToRaycast(Controls::mouse_position, glm::ivec2(window_width, window_height), camera.inv_vp);
    return raycastEntities(raycast, entity_manager);
}

Raycast raycastColliderWithMouse(const Camera& camera, EntityManager& entity_manager) {
    auto raycast = mouseToRaycast(Controls::mouse_position, glm::ivec2(window_width, window_height), camera.inv_vp);
    return raycastEntities(raycast, entity_manager, true);
}

void handleEditorControls(EntityManager*& entity_manager, AssetManager& asset_manager, float dt) {
    static float mouse_hold_threshold = 0.1; // The time past which a mouse press is considered a hold

    ImGuiIO& io = ImGui::GetIO();
    Camera& camera = *Cameras::get_active_camera();

    if (Controls::editor.isAction("toggle_terminal"))
        Editor::do_terminal = !Editor::do_terminal;

    if (!io.WantCaptureKeyboard) {
        if (Controls::editor.isAction("delete_selection") && Editor::selection.ids.size()) {
            for (auto& id : Editor::selection.ids) {
                entity_manager->deleteEntity(id);
                if (id == entity_manager->water) {
                    entity_manager->water = NULLID;
                }
            }
            Editor::selection.clear();
        }
        if (Controls::editor.isAction("toggle_collider_visibility"))
            Editor::draw_colliders = !Editor::draw_colliders;

        if (Controls::editor.isAction("switch_cameras"))
            Editor::use_level_camera = !Editor::use_level_camera;

        if (Controls::editor.isAction("switch_editor_mode")) {
            Editor::editor_mode = (EditorMode)(((int)Editor::editor_mode + 1) % (int)EditorMode::NUM);
            Editor::selection.clear();
        }

        if (Controls::editor.isAction("copy") &&
            Editor::selection.ids.size() && !(Editor::selection.type & WATER_ENTITY)) {
            referenceToCopySelection(*entity_manager, Editor::selection, Editor::copy_selection);
            pushInfoMessage("Copied selection");
        }

        if (Controls::editor.isAction("paste") && Editor::copy_selection.entities.size() != 0) {
            Editor::selection.clear();
            createCopySelectionEntities(*entity_manager, asset_manager, Editor::copy_selection, Editor::selection);
        }

        if (Controls::editor.isAction("toggle_debug_visibility")) {
            Editor::draw_debug_wireframe = !Editor::draw_debug_wireframe;
            pushInfoMessage(Editor::draw_debug_wireframe ? "Debug wireframe on" : "Debug wireframe off");
        }

        if (Controls::editor.isAction("focus_camera") && Editor::selection.ids.size()) {
            camera.set_target(Editor::selection.avg_position);
            pushInfoMessage("Refocusing camera");
        }


        if (Controls::editor.isAction("switch_camera_mode")) {
            if (camera.state == Camera::Type::TRACKBALL) {
                camera.state = Camera::Type::STATIC;
                pushInfoMessage("Static camera");
            }
            else if (camera.state == Camera::Type::SHOOTER) {
                camera.state = Camera::Type::TRACKBALL;
                pushInfoMessage("Orbit camera");
            }
            else if (camera.state == Camera::Type::STATIC) {
                camera.state = Camera::Type::SHOOTER;

                Controls::mouse_position = glm::dvec2(window_width / 2, window_height / 2);
                Controls::delta_mouse_position = glm::dvec2(0, 0);
                pushInfoMessage("Shooter style camera");
            }
        }

        if (Controls::editor.isAction("toggle_playing")) {
            playGame();
            pushInfoMessage("Playing");
        }

        if (Controls::editor.isAction("toggle_level_camera_visibility")) {
            Editor::draw_level_camera = !Editor::draw_level_camera;
            pushInfoMessage(Editor::draw_level_camera ? "Camera wireframe on" : "Camera wireframe off");
        }
    }

    if (!io.WantCaptureMouse) {
        if (Controls::editor.right_mouse.state == Controls::RELEASE && (glfwGetTime() - Controls::editor.right_mouse.last_press) < mouse_hold_threshold) {
            if (Editor::editor_mode == EditorMode::COLLIDERS) {
                glm::vec3 n;
                auto raycast = raycastColliderWithMouse(camera, *entity_manager);
                if (raycast.result.hit) {
                    entity_manager->deleteEntity(entity_manager->entities[raycast.result.indice]->id);
                }
            }
            else {
                Editor::selection.clear();
            }
        }
        if (Controls::editor.left_mouse.state == Controls::RELEASE && (glfwGetTime() - Controls::editor.left_mouse.last_press) < mouse_hold_threshold) {
            switch (Editor::editor_mode)
            {
            case EditorMode::ENTITY:
            {
                auto raycast = raycastEntityWithMouse(camera, *entity_manager);
                if (raycast.result.hit) {
                    if (!glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)) {
                        Editor::selection.clear();
                    }

                    auto pick_e = entity_manager->entities[raycast.result.indice];
                    Editor::selection.toggleEntity(*entity_manager, pick_e);
                } else {
                    Editor::selection.clear();
                }
                break;
            }
            case EditorMode::COLLIDERS:
            {
                auto raycast = raycastColliderWithMouse(camera, *entity_manager);
                if (raycast.result.hit) {
                    auto pick_c = (ColliderEntity*)entity_manager->entities[raycast.result.indice];

                    std::cout << "Collided, normal: " << raycast.result.normal << "\n";
                    auto c = (ColliderEntity*)copyEntity((Entity*)pick_c);
                    c->id = entity_manager->getFreeId();
                    c->mesh = c->mesh;

                    c->position = pick_c->position + raycast.result.normal;
                    c->scale = pick_c->scale;
                    c->rotation = pick_c->rotation;
                    c->collider_position = pick_c->collider_position + raycast.result.normal;
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

    bool camera_movement_active = !Editor::transform_active && !io.WantCaptureMouse && !io.WantCaptureKeyboard;
    if (camera.state == Camera::Type::TRACKBALL && camera_movement_active) {
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
    else if (camera.state == Camera::Type::SHOOTER && camera_movement_active) {
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

            auto camera_look = camera.position - camera.target;

            auto rotation_x = glm::rotate(glm::mat4x4(1.0f), x_angle, camera.up);
            glm::mat4 rotation_y(1.0f);

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
                rotation_y = glm::rotate(glm::mat4x4(1.0f), y_angle, camera.right);
            }

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
        Editor::do_terminal = !Editor::do_terminal;

    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureKeyboard) {
        if (Controls::editor.isAction("toggle_playing")) {
            pauseGame();
            pushInfoMessage("Switched to editor");
        }
        if (Controls::game.isAction("reset")) {
            resetGameState();
            pushInfoMessage("Resetting");
        }
        if (Controls::game.isAction("toggle_animation_state_visibility"))
            Editor::debug_animations = !Editor::debug_animations;

        if (Controls::game.isAction("slow_time")) {
            true_time_warp *= 0.8;
            pushInfoMessage("Time warp " + std::to_string(true_time_warp), InfoMessage::Urgency::NORMAL, 0.25, "#time-warp");
        }
        if (Controls::game.isAction("fast_time")) {
            true_time_warp *= 1.2;
            pushInfoMessage("Time warp " + std::to_string(true_time_warp), InfoMessage::Urgency::NORMAL, 0.25, "#time-warp");
        }

        if (Controls::game.isAction("pause"))
            true_time_pause = !true_time_pause;

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