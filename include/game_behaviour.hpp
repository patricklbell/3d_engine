#ifndef ENGINE_GAME_BEHAVIOUR_HPP
#define ENGINE_GAME_BEHAVIOUR_HPP

#include "entities.hpp"
#include "graphics.hpp"

void pauseGame();
void playGame();
void resetGameState();

void initCameraMove(glm::vec3 origin, glm::vec3 target, float duration);
void updateCameraMove(float dt);
void updatePlayerEntity(EntityManager& entity_manager, float dt, PlayerEntity& player);

struct GameState {
	Level level;
	bool is_active = false;
	bool paused = false;
	bool initialized = false;
};

extern GameState gamestate;

#endif // ENGINE_GAME_BEHAVIOUR_HPP
