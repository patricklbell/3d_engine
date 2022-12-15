#ifndef GAME_BEHAVIOUR_HPP
#define GAME_BEHAVIOUR_HPP

#include "entities.hpp"
#include "graphics.hpp"

void pauseGame();
void playGame();
void resetGameState();
void updateGameEntities(float dt, EntityManager& entity_manager);

struct GameState {
	Level level;
	bool is_active = false;
	bool paused = false;
	bool initialized = false;
};

extern GameState gamestate;

#endif // GAME_BEHAVIOUR_HPP
