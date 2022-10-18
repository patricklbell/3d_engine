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

void updatePlayerEntity(float dt, PlayerEntity *player) {
    // Speed up animations if there are multiple queued
    player->time_scale_mult = 1.0f + ((float)player->actions.size() / (float)player->MAX_ACTION_BUFFER) * player->MAX_ACTION_SPEEDUP;

    // Set default animation to idle, @todo make init
    if (player->default_event.animation == nullptr) {
        auto event = player->play("Armature|IDLE", 0.0f, true);
        event->blend = false;
    }

    if (player->actions.size() > 0) {
        auto& a = player->actions[0];

        if (!a.active) {
            a.active = true;

            // Play animations that accompany actions
            if (player->animesh != nullptr) {
                auto anim_name = getPlayerActionAnimationName(a.type);
                

                auto event = player->play(anim_name, 0.0f);
                event->delta_position = a.delta_position;
                event->delta_rotation = a.delta_rotation;
                event->delta_transform = glm::mat4_cast(a.delta_rotation) * glm::translate(glm::mat4(1.0), a.delta_position * glm::inverse(player->scale));

                event->transform_animation = true;
                event->transform_entity = true;
                event->blend = true;
                event->time_scale = event->duration / a.duration;
            }
        }

        a.time += dt * player->time_scale_mult;
        if (a.time >= a.duration - a.duration*0.1) {
            player->actions.erase(player->actions.begin());
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
}

void playGame(EntityManager* &entity_manager) {
    playing = true;

    if(!has_played) {
        resetGameEntities();
    }
    entity_manager = &game_entity_manager;

    game_camera = level_camera;
    auto look_dir = glm::normalize(game_camera.target - game_camera.position);
    initCameraMove(game_camera.position - look_dir*4.0f, game_camera.position, 1.5f);

    has_played = true;
}

void updateGameEntities(float dt, EntityManager* &entity_manager) {
    updateCameraMove(dt);

    if (entity_manager->player != NULLID) {
        auto player = (PlayerEntity*)entity_manager->getEntity(entity_manager->player);
        if(player != nullptr) 
            updatePlayerEntity(dt, player);
    }
}
