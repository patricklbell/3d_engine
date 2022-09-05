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
#include "assets.hpp"

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
                        camera.target = w_e->position;
                    }
                }
            }
        }
        else if (e->type & EntityType::MESH_ENTITY) {
            auto m_e = (MeshEntity*)e;
            if (m_e->mesh == nullptr) continue;

            const auto& mesh = m_e->mesh;
            const auto g_trans = createModelMatrix(m_e->position, m_e->rotation, m_e->scale);

            for (int j = 0; j <= mesh->num_indices - 3; j += 3) {
                if (mesh->indices[j    ] >= mesh->num_vertices ||
                    mesh->indices[j + 1] >= mesh->num_vertices ||
                    mesh->indices[j + 2] >= mesh->num_vertices)
                    continue;
                const auto p1 = mesh->vertices[mesh->indices[j]    ];
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
                            closest_e = m_e;
                            camera.target = m_e->position;
                        }
                    }
                }
            }
        }
    }

    return closest_e;
}

ColliderEntity* pickColliderWithMouse(Camera& camera, EntityManager& entity_manager, glm::vec3 &normal, bool only_selectable=false) {
    glm::vec3 out_origin;
    glm::vec3 out_direction;
    screenPosToWorldRay(mouse_position, camera.view, camera.projection, out_origin, out_direction);

    float min_collision_distance = std::numeric_limits<float>::max();
    std::cout << "Pick collider\n";

    float min_t;
    ColliderEntity* nearest_c = nullptr;
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        auto c = (ColliderEntity*)entity_manager.entities[i];
        if (c == nullptr || !(c->type & COLLIDER_ENTITY)) continue;
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

void handleEditorControls(Camera &editor_camera, Camera &level_camera, EntityManager &entity_manager, AssetManager &asset_manager, float dt) {
    // Stores the previous state of input, updated at end of function
    static bool c_key_prev               = false;
    static bool p_key_prev               = false;
    static bool d_key_prev               = false;
    static bool f_key_prev               = false;
    static bool b_key_prev               = false;
    static bool delete_key_prev          = false;
    static bool mouse_left_prev          = false;
    static bool mouse_right_prev         = false;
    static bool ctrl_v_prev              = false;
    static bool backtick_key_prev        = false;

    static double mouse_left_press_time = glfwGetTime();
    static glm::vec3 shooter_camera_velocity = glm::vec3(0.0);
    static float shooter_camera_deceleration = 0.8;

    bool camera_movement_active = !editor::transform_active;
    ImGuiIO& io = ImGui::GetIO();
    controls::left_mouse_click_press   = !io.WantCaptureMouse && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT )   && !mouse_left_prev;
    controls::left_mouse_click_release = !io.WantCaptureMouse && !glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT )  && mouse_left_prev;
    controls::right_mouse_click_press  = !io.WantCaptureMouse && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT)   && !mouse_right_prev;
    controls::right_mouse_click_release= !io.WantCaptureMouse && !glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT)  && mouse_right_prev;

    // Unlike other inputs, calculate delta but update mouse position immediately
    glm::dvec2 delta_mouse_position = mouse_position;
    glfwGetCursorPos(window, &mouse_position.x, &mouse_position.y);
    delta_mouse_position = mouse_position - delta_mouse_position;

    if(glfwGetKey(window, GLFW_KEY_GRAVE_ACCENT) && !backtick_key_prev) 
        editor::do_terminal = !editor::do_terminal;
    
    if (!io.WantCaptureKeyboard && glfwGetKey(window, GLFW_KEY_C) && !c_key_prev
        && !glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) && !glfwGetKey(window, GLFW_KEY_LEFT_SHIFT))
        editor::use_level_camera = !editor::use_level_camera;
    
    if (!io.WantCaptureKeyboard && glfwGetKey(window, GLFW_KEY_B) && !b_key_prev) {
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)) {
            editor::draw_colliders = !editor::draw_colliders;
        }
        else {
            editor::editor_mode = (EditorMode)(((int)editor::editor_mode + 1) % (int)EditorMode::NUM);
            editor::selection.clear();
        }
    }

    Camera* camera_ptr = &editor_camera;
    if (editor::use_level_camera)
        camera_ptr = &level_camera;
    Camera& camera = *camera_ptr;

    if(!io.WantCaptureKeyboard){
        if (editor::selection.ids.size() && glfwGetKey(window, GLFW_KEY_DELETE) && !delete_key_prev) {
            for (auto& id : editor::selection.ids) {
                entity_manager.deleteEntity(id);
                if (id == entity_manager.water) {
                    entity_manager.water = NULLID;
                }
            }
            editor::selection.clear();
        }
        if (editor::selection.ids.size() && !(editor::selection.type & WATER_ENTITY)
            && glfwGetKey(window, GLFW_KEY_C) && !c_key_prev && glfwGetKey(window, GLFW_KEY_LEFT_CONTROL)) {
            referenceToCopySelection(entity_manager, editor::selection, editor::copy_selection);
        }
        if (editor::copy_selection.entities.size() != 0 && 
            !ctrl_v_prev && glfwGetKey(window, GLFW_KEY_V) && glfwGetKey(window, GLFW_KEY_LEFT_CONTROL)) {
            editor::selection.clear();
            createCopySelectionEntities(entity_manager, asset_manager, editor::copy_selection, editor::selection);
            updateCameraTarget(camera, editor::selection.avg_position);
        }
        if (glfwGetKey(window, GLFW_KEY_D) && !d_key_prev && glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)) {
            editor::draw_debug_wireframe = !editor::draw_debug_wireframe;
        }
        if (glfwGetKey(window, GLFW_KEY_C) && !c_key_prev
            && !glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) && glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)) {
            if (camera.state == Camera::TYPE::TRACKBALL) {
                camera.state = Camera::TYPE::STATIC;
            }
            else if (camera.state == Camera::TYPE::SHOOTER) {
                camera.state = Camera::TYPE::TRACKBALL;
                if (editor::selection.ids.size() == 1) {
                    updateCameraTarget(camera, editor::selection.avg_position);
                }
            }
            else if (camera.state == Camera::TYPE::STATIC) {
                camera.state = Camera::TYPE::SHOOTER;

                mouse_position = glm::dvec2(window_width / 2, window_height / 2);
                delta_mouse_position = glm::dvec2(0, 0);
            }
        }

        if (glfwGetKey(window, GLFW_KEY_P) && !p_key_prev)
            playing = !playing;

        if (glfwGetKey(window, GLFW_KEY_F) && !f_key_prev)
            editor::draw_level_camera = !editor::draw_level_camera;
    }

    if (right_mouse_click_release) {
        if (editor::editor_mode == EditorMode::COLLIDERS) {
            auto collider = pickColliderWithMouse(camera, entity_manager, glm::vec3());
            if (collider != nullptr) {
                entity_manager.deleteEntity(collider->id);
            }
        }
        else {
            editor::selection.clear();
        }
    }
    if (left_mouse_click_release && (glfwGetTime() - mouse_left_press_time) < 0.2) {
        switch (editor::editor_mode)
        {
        case EditorMode::ENTITY:
        {
            auto e = pickEntityWithMouse(camera, entity_manager);
            if (e != nullptr) {
                if (!glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)) {
                    editor::selection.clear();
                }
                editor::selection.toggleEntity(entity_manager, e);

                if (camera.state != Camera::TYPE::STATIC && editor::selection.avg_position_count) {
                    updateCameraTarget(camera, editor::selection.avg_position);
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
            auto pick_c = pickColliderWithMouse(camera, entity_manager, normal);
            if (pick_c != nullptr) {
                std::cout << "Collided, normal: " << normal << "\n";
                auto c = (ColliderEntity*)copyEntity(pick_c);
                c->id = entity_manager.getFreeId();
                c->mesh = pick_c->mesh;
                
                c->position = pick_c->position + 2.0f * normal;
                c->scale    = pick_c->scale;
                c->rotation = pick_c->rotation;
                c->collider_position = pick_c->collider_position + 2.0f * normal;
                c->collider_scale = pick_c->collider_scale;
                c->collider_rotation = pick_c->collider_rotation;

                entity_manager.setEntity(c->id.i, c);
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
        if (!io.WantCaptureMouse && !(left_mouse_click_release && (glfwGetTime() - mouse_left_press_time) < 0.2) && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) && camera_movement_active) {
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
            if (!glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)) {
                if (glfwGetKey(window, GLFW_KEY_W)) {
                    shooter_camera_velocity += camera.forward * camera_movement_acceleration;
                }
                if (glfwGetKey(window, GLFW_KEY_S)) {
                    shooter_camera_velocity -= camera.forward * camera_movement_acceleration;
                }
                if (glfwGetKey(window, GLFW_KEY_D)) {
                    shooter_camera_velocity += camera.right * camera_movement_acceleration;
                }
                if (glfwGetKey(window, GLFW_KEY_A)) {
                    shooter_camera_velocity -= camera.right * camera_movement_acceleration;
                }
            }
            if (glfwGetKey(window, GLFW_KEY_SPACE)) {
                if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL)) {
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

    ctrl_v_prev       = glfwGetKey(window, GLFW_KEY_V) && glfwGetKey(window, GLFW_KEY_LEFT_CONTROL);
    backtick_key_prev  = glfwGetKey(window, GLFW_KEY_GRAVE_ACCENT);
    delete_key_prev    = glfwGetKey(window, GLFW_KEY_DELETE);
    c_key_prev         = glfwGetKey(window, GLFW_KEY_C);
    d_key_prev         = glfwGetKey(window, GLFW_KEY_D);
    f_key_prev         = glfwGetKey(window, GLFW_KEY_F);
    p_key_prev         = glfwGetKey(window, GLFW_KEY_P);
    b_key_prev         = glfwGetKey(window, GLFW_KEY_B);

    if(mouse_left_prev == GLFW_RELEASE && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) mouse_left_press_time = glfwGetTime();
    mouse_left_prev    = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
    mouse_right_prev   = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
}

static struct CollisionNeighbours {
    std::unordered_map<glm::ivec3, Id> loc_to_id;
    std::vector<Id> psuedo_entities;
};

// This is obviously slow, when logic becomes more complicated change
static CollisionNeighbours findColliderNeighbours(EntityManager &entity_manager, ColliderEntity* c) {
    CollisionNeighbours neighbours;
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        auto c_e = (ColliderEntity*)entity_manager.entities[i];
        if (c == c_e || c_e == nullptr || !(c_e->type & EntityType::COLLIDER_ENTITY)) continue;

        for (int x = -1; x <= 1; ++x) {
            for (int y = -1; y <= 1; ++y) {
                for (int z = -1; z <= 1; ++z) {
                    auto offset = glm::ivec3(x, y, z);
                    if (glm::ivec3(c->collider_position) == glm::ivec3(c_e->collider_position) + 2*offset) {
                        neighbours.loc_to_id[offset] = c_e->id;
                    }
                }
            }
        }
    }
    return neighbours;
}

static void addPsuedoNeighbours(EntityManager& entity_manager, AssetManager& asset_manager, ColliderEntity* c, CollisionNeighbours& neighbours) {
    auto psuedo_mesh = asset_manager.getMesh("data/mesh/o_wire.mesh");
    if (psuedo_mesh == nullptr) {
        psuedo_mesh = asset_manager.createMesh("data/mesh/o_wire.mesh");
        if (!asset_manager.loadMesh(psuedo_mesh, "data/mesh/o_wire.mesh", true)) {
            std::cerr << "Failed to load psuedo_mesh at path data/mesh/o_wire.mesh in addPsuedoNeighbours\n";
            return;
        }
    }
    for (auto& p : neighbours.loc_to_id) {
        std::cout << "Neighbour at position " << glm::vec3(p.first) << "\n";
    }

    for (int i = 0; i < 2; ++i) {
        for (int xz = -1; xz <= 1; xz += 2) {
            auto offset = glm::ivec3((i == 0) * xz, 0, (i == 1) * xz);
            std::cout << "Testing offset " << glm::vec3(offset) << "\n";
            // There is either a collider there or not one below
            if (neighbours.loc_to_id.find(offset)                       != neighbours.loc_to_id.end() ||
                neighbours.loc_to_id.find(offset + glm::ivec3(0, 1, 0)) == neighbours.loc_to_id.end()) 
                continue;
            std::cout << "Passed\n";

            auto psuedo = (ColliderEntity*)entity_manager.createEntity(COLLIDER_ENTITY);
            psuedo->mesh = psuedo_mesh;
            // @note uses collider position for mesh position as it assume o_wire is correctly placed
            psuedo->position          = c->collider_position - glm::vec3(2*offset);
            psuedo->collider_position = c->collider_position - glm::vec3(2*offset);
            psuedo->selectable = true;
            neighbours.psuedo_entities.emplace_back(psuedo->id);
        }
    }
}

static bool isPickPsuedo(ColliderEntity* c, CollisionNeighbours& neighbours) {
    for (const auto& id : neighbours.psuedo_entities) {
        std::cout << "Comparing id " << id.i << " and " << c->id.i << "\n";
        if (id == c->id) 
            return true;
    }
    return false;
}

static void clearNeighbour(EntityManager& entity_manager, CollisionNeighbours& neighbours) {
    for (auto& id : neighbours.psuedo_entities) {
        entity_manager.deleteEntity(id);
    }
    neighbours.psuedo_entities.clear();
}

void handleGameControls(Camera& camera, EntityManager& entity_manager, AssetManager& asset_manager, float dt) {
    static bool p_key_prev          = false;
    static bool backtick_key_prev   = false;
    static bool mouse_left_prev     = false;

    ImGuiIO& io = ImGui::GetIO();
    controls::left_mouse_click_press = !io.WantCaptureMouse && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) && !mouse_left_prev;
    // Unlike other inputs, calculate delta but update mouse position immediately
    glm::dvec2 delta_mouse_position = mouse_position;
    glfwGetCursorPos(window, &mouse_position.x, &mouse_position.y);
    delta_mouse_position = mouse_position - delta_mouse_position;

    if (glfwGetKey(window, GLFW_KEY_P) && !p_key_prev)
        playing = !playing;
    if (glfwGetKey(window, GLFW_KEY_GRAVE_ACCENT) && !backtick_key_prev)
        editor::do_terminal = !editor::do_terminal;

    // 
    // Selection and Actions
    //
    static Id selected_id = NULLID;
    static CollisionNeighbours selected_neighbours;
    
    if (controls::left_mouse_click_press) {
        auto collider = pickColliderWithMouse(camera, entity_manager, glm::vec3(), true);

        if (collider != nullptr) {
            if (isPickPsuedo(collider, selected_neighbours)) {
                clearNeighbour(entity_manager, selected_neighbours);

                auto s = (ColliderEntity*)entity_manager.getEntity(selected_id);
                auto offset = collider->collider_position - s->collider_position;
                s->collider_position += offset;
                s->position += offset;

                selected_neighbours = findColliderNeighbours(entity_manager, collider);
                addPsuedoNeighbours(entity_manager, asset_manager, s, selected_neighbours);
            }
            else if (collider->id != selected_id){
                selected_id = collider->id;
                selected_neighbours = findColliderNeighbours(entity_manager, collider);
                addPsuedoNeighbours(entity_manager, asset_manager, collider, selected_neighbours);
            }
        }
        else {
            clearNeighbour(entity_manager, selected_neighbours);
            selected_id = NULLID;
        }
    }

    // For now just use wireframe, in future some kind of edge detection, or saturation effect
    auto s = (ColliderEntity*)entity_manager.getEntity(selected_id);
    if (s != nullptr && (s->type & COLLIDER_ENTITY) && s->mesh != nullptr) {
        drawMeshWireframe(*s->mesh, s->position, s->rotation, s->scale, camera, true);
    }

    backtick_key_prev   = glfwGetKey(window, GLFW_KEY_GRAVE_ACCENT);
    p_key_prev          = glfwGetKey(window, GLFW_KEY_P);
    mouse_left_prev     = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
}