#ifndef LIGHTMAPPER_HPP
#define LIGHTMAPPER_HPP

#include "entities.hpp"

bool runLightmapper(EntityManager& entity_manager, AssetManager& asset_manager, Texture* skybox, Texture* skybox_irradiance, Texture* skybox_specular);

#endif