#ifndef GAME_BEHAVIOUR_HPP
#define GAME_BEHAVIOUR_HPP

#include "entities.hpp"
#include "graphics.hpp"

void pauseGame(EntityManager* &entity_manager);
void resetGameEntities();
void playGame(EntityManager* &entity_manager);
void updateGameEntities(float dt, EntityManager* &entity_manager);

namespace Game {
	extern graphics::FogProperties fog_properties;
}

#endif // GAME_BEHAVIOUR_HPP
