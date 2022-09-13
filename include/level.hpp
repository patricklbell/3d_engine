#ifndef LEVEL_HPP
#define LEVEL_HPP

#include "graphics.hpp"

void saveLevel(EntityManager& entity_manager, const std::string& level_path, const Camera &camera);
bool loadLevel(EntityManager &entity_manager, AssetManager &asset_manager, const std::string &level_path, Camera& camera);

#endif // LEVEL_HPP
