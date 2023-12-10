#ifndef LEVEL_HPP
#define LEVEL_HPP

#include "graphics.hpp"
#include "entities.hpp"
#include "assets.hpp"
#include "physics.hpp"

#include <Jolt/Jolt.h>
#include "Jolt/Physics/PhysicsSystem.h"
#include "Jolt/Physics/PhysicsScene.h"

struct FogProperties {
    float anisotropy = 0.2;
    float density = 0.03;
    float noise_scale = 0.15;
    float noise_amount = 0.3;
};

struct Environment {
    Texture* skybox = nullptr;
    Texture* skybox_irradiance = nullptr;
    Texture* skybox_specular = nullptr;

    glm::vec3 sun_color = glm::normalize(glm::vec3(-1, -1, -0.25));
    glm::vec3 sun_direction = 5.0f * glm::vec3(0.941, 0.933, 0.849);

    FogProperties fog;
};

struct Level {
    std::string path = "";

    EntityManager entities;
    Camera camera;
    Environment environment;

    enum class Type : uint64_t {
        BASIC = 0,
    } type = Type::BASIC;
};

extern Level loaded_level;

void initDefaultLevel(Level& level, AssetManager& assets);
bool saveLevel(Level& level, const std::string& path);
bool loadLevel(Level& level, AssetManager& assets, const std::string& path);

#endif // LEVEL_HPP
