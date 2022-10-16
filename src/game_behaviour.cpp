#include "entities.hpp"
#include "graphics.hpp"
#include "utilities.hpp"
#include <iostream>

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
    if (player->actions.size() > 0) {
        auto& a = player->actions[0];
        if (!a.active) {
            a.active = true;
            a.beg_position = player->position;
            a.beg_rotation = player->rotation;

            // Play animations that accompany actions
            auto animation_name1 = getPlayerActionAnimationName(a.type);
            std::string animation_name2;
            if (player->actions.size() > 1)
                animation_name2 = getPlayerActionAnimationName(player->actions[1].type);
            else
                animation_name2 = "Armature|IDLE";

            float animation_speed1 = player->getAnimationDuration(animation_name1) / a.duration;
            float animation_speed2;
            if (player->actions.size() > 1)
                animation_speed2 = player->getAnimationDuration(animation_name2) / player->actions[1].duration;
            else
                animation_speed2 = 1.0;

            player->playBlended(animation_name1, 0.0, animation_speed1, 
                                animation_name2, 0.0, animation_speed2, 
                                createModelMatrix(a.delta_position, a.delta_rotation, glm::vec3(1.0)),
                                0.8, false);
        }

        float t = glm::smoothstep(0.0f, a.duration, a.time);

        // Speed up actions if there are multiple queued
        float speed_factor = 1.0f + ((float)player->actions.size() / (float)player->MAX_ACTION_BUFFER) * player->MAX_ACTION_SPEEDUP;
        a.time += dt*speed_factor;

        if (t >= 1.0f) {
            player->rotation *= a.delta_rotation;
            player->position += player->rotation * a.delta_position;

            player->play("Armature|IDLE", 0.0, 1.0, true);
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
