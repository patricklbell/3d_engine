#include "entities.hpp"
#include "graphics.hpp"

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

void updateGameEntities(float dt) {
    updateCameraMove(dt);
}
