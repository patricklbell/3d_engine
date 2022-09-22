#ifndef GAME_BEHAVIOUR_HPP
#define GAME_BEHAVIOUR_HPP

#include "entities.hpp"

void pauseGame(EntityManager* &entity_manager);
void resetGameEntities();
void playGame(EntityManager* &entity_manager);
void updateGameEntities(float dt);

#endif // GAME_BEHAVIOUR_HPP
