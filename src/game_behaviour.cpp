#include <iostream>
#include <glm/gtc/matrix_transform.hpp>

#include <camera/globals.hpp>

#include <Jolt/Jolt.h>
#include "Jolt/Math/Real.h"

#include <utilities/math.hpp>
#include "entities.hpp"
#include "level.hpp"

#include "game_behaviour.hpp"
#include "physics.hpp"

GameState gamestate;

// @todo move to camera class, in future with multiple control points, etc.
glm::vec3 camera_move_target, camera_move_origin;
float camera_move_time, camera_move_duration;
bool do_camera_move = false;
float initial_fog_density;

void initCameraMove(glm::vec3 origin, glm::vec3 target, float duration) {
    do_camera_move = true;
    camera_move_target = target;
    camera_move_origin = origin;

    camera_move_duration = duration;
    camera_move_time = 0.0f;

    const auto& env = gamestate.level.environment;
    initial_fog_density = env.fog.density;
}

void updateCameraMove(float dt) {
    if(do_camera_move) {
        camera_move_time += dt;
        float t = glm::smoothstep(0.0f, camera_move_duration, camera_move_time);
        t = sqrt(t);
        gamestate.level.camera.set_position(glm::mix(camera_move_origin, camera_move_target, t));

        auto& env = gamestate.level.environment;
        env.fog.density = glm::mix(10 * initial_fog_density, initial_fog_density, t);

        do_camera_move = camera_move_time <= camera_move_duration;
    }
}

static std::string getPlayerActionAnimationName(PlayerActionType& type) {
    switch (type)
    {
    case PlayerActionType::STEP_FORWARD:
        return "Armature|STEP";
    case PlayerActionType::TURN_LEFT:
        return "Armature|TURN_LEFT";
    case PlayerActionType::TURN_RIGHT:
        return "Armature|TURN_RIGHT";
    default:
        return "Unknown Action " + std::to_string((uint64_t)type);
    }
}

// @todo octree storage of colliders
bool isPositionFree(EntityManager &entity_manager, glm::vec3 pos) {
    bool floor = false;

    for (int i = 0; i < ENTITY_COUNT; ++i) {
        auto c = (ColliderEntity*)entity_manager.entities[i];
        if (c == nullptr || !entityInherits(c->type, COLLIDER_ENTITY)) continue;

        if (glm::round(c->collider_position) == glm::round(pos)) 
            return false;

        if (glm::round(c->collider_position) == glm::round(pos - glm::vec3(0, 1, 0)))
            floor = true;
    }
    return floor;
}

void updatePlayerEntity(EntityManager& entity_manager, float dt, PlayerEntity &player) {
    // Speed up animations if there are multiple queued
    player.time_scale_mult = 1.0f + ((float)player.actions.size() / (float)MAX_ACTION_BUFFER) * MAX_ACTION_SPEEDUP;

    // Set default animation to idle, @todo make init
    if (player.default_event.animation == nullptr) {
        auto event = player.play("Armature|IDLE", 0.0f, true);
        event->blend = false;
    }

    if (player.actions.size() > 0) {
        auto& a = player.actions[0];

        bool invalid_action = false;
        if (!a.active) {
            a.active = true;

            auto target_position = player.position + player.gizmo_position_offset + player.rotation * a.delta_rotation * a.delta_position;
            if (isPositionFree(entity_manager, target_position)) {
                // Play animations that accompany actions
                std::cout << "Playing animation " << getPlayerActionAnimationName(a.type) << "\n";
                auto event = player.play(getPlayerActionAnimationName(a.type));
                if (event != nullptr) {
                    event->delta_position = a.delta_position;
                    event->delta_rotation = a.delta_rotation;

                    event->transform_entity = true;
                    event->blend = true;
                    event->time_scale = event->duration / a.duration;
                }
            }
            else {
                player.actions.erase(player.actions.begin());
                invalid_action = true;
            }

        }
        // Blend with next actions
        //if (player.actions.size() > 1 && a.time >= a.duration * 0.8) {
        if (!invalid_action && player.actions.size() > 1) {
            auto& b = player.actions[1];
            if (!b.active) {
                b.active = true;

                auto target_position = player.position + player.gizmo_position_offset + player.rotation * a.delta_rotation * (a.delta_position + b.delta_rotation * b.delta_position);
                if (isPositionFree(entity_manager, target_position)) {
                    std::cout << "Playing preemptive animation " << getPlayerActionAnimationName(b.type) << "\n";
                    auto event = player.play(getPlayerActionAnimationName(b.type));
                    if (event != nullptr) {
                        event->delta_position = b.delta_position;
                        event->delta_rotation = b.delta_rotation;

                        event->transform_entity = true;
                        event->blend = true;
                        event->time_scale = event->duration / b.duration;
                    }
                }
                else {
                    player.actions.erase(player.actions.begin()++);
                }
            }
        }

        if (!invalid_action) {
            a.time += dt * player.time_scale_mult;
            if (a.time >= a.duration) {
                player.actions.erase(player.actions.begin());
            }
        }
    }
}

void pauseGame() {
    gamestate.is_active = false;
}

void resetGameState() {
    gamestate.level = loaded_level;
    auto look_dir = glm::normalize(gamestate.level.camera.target - gamestate.level.camera.position);
    initCameraMove(gamestate.level.camera.position - look_dir * 6.0f, gamestate.level.camera.position, 1.2f);

    if (physics::system != nullptr) delete physics::system;
    physics::system = new JPH::PhysicsSystem();
    physics::system->Init(
        1024, 0, 1024, 1024, 
        physics::broad_phase_layer_interface, physics::object_vs_broadphase_layer_filter, physics::object_vs_object_layer_filter
    );

    auto &bi = physics::system->GetBodyInterface();
    JPH::BodyIDVector body_ids;
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        auto e = gamestate.level.entities.entities[i];
        if (e == nullptr) continue;

        if (entityInherits(e->type, EntityType::MESH_ENTITY)) {
            auto me = reinterpret_cast<MeshEntity*>(e);

            if (me->body_settings != nullptr) {
                auto body = bi.CreateBody(*me->body_settings);
                if (body == nullptr) {
                    std::cerr << "Failed to add body to physics system, maybe not enough storage?\n";
                    continue;
                }

                body->SetUserData(me->id.to_phys());
                me->body_id = body->GetID();
                body_ids.push_back(body->GetID());
            }
        }
    }
	auto add_state = bi.AddBodiesPrepare(body_ids.data(), (int)body_ids.size());
	bi.AddBodiesFinalize(body_ids.data(), (int)body_ids.size(), add_state, JPH::EActivation::Activate);

    gamestate.initialized = true;
}

void playGame() {
    if (!gamestate.initialized) {
        resetGameState();
    }

    gamestate.is_active = true;
}
