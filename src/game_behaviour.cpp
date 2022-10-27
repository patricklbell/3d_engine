#include "entities.hpp"
#include "graphics.hpp"
#include "utilities.hpp"
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>

glm::vec3 camera_move_target, camera_move_origin;
float camera_move_time, camera_move_duration;
bool do_camera_move = false;
void initCameraMove(glm::vec3 origin, glm::vec3 target, float duration) {
    do_camera_move = true;
    camera_move_target = target;
    camera_move_origin = origin;

    camera_move_duration = duration;
    camera_move_time = 0.0f;
}

void updateCameraMove(float dt) {
    if(do_camera_move) {
        camera_move_time += dt;
        float t = glm::smoothstep(0.0f, camera_move_duration, camera_move_time);
        t = sqrt(t);
        game_camera.position = glm::mix(camera_move_origin, camera_move_target, t);
        updateCameraView(game_camera);

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

void pauseGame(EntityManager* &entity_manager) {
    playing = false;

    entity_manager = &level_entity_manager;
}

void resetGameEntities() {
    game_entity_manager.clear();
    game_entity_manager = level_entity_manager;
    level_entity_manager.copyEntities(game_entity_manager.entities);

    game_camera = level_camera;
    auto look_dir = glm::normalize(game_camera.target - game_camera.position);
    initCameraMove(game_camera.position - look_dir * 4.0f, game_camera.position, 1.5f);
}

void playGame(EntityManager* &entity_manager) {
    playing = true;

    if(!has_played) {
        resetGameEntities();
    }
    entity_manager = &game_entity_manager;

    has_played = true;
}

void updateGameEntities(float dt, EntityManager* &entity_manager) {
    updateCameraMove(dt);

    if (entity_manager->player != NULLID) {
        auto player = (PlayerEntity*)entity_manager->getEntity(entity_manager->player);
        if(player != nullptr) 
            updatePlayerEntity(*entity_manager, dt, *player);
    }
}
