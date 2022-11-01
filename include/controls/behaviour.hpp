#ifndef CONTROLS_BEHAVIOUR_HPP
#define CONTROLS_BEHAVIOUR_HPP

#include <assets.hpp>
#include <entities.hpp>

void handleGameControls(EntityManager*& entity_manager, AssetManager& asset_manager, float dt);
void handleEditorControls(EntityManager*& entity_manager, AssetManager& asset_manager, float dt);

#endif // CONTROLS_BEHAVIOUR_HPP