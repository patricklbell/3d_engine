#include <set>
#include <iostream>

#include <camera/core.hpp>
#include <utilities/strings.hpp>
#include "entities.hpp"
#include "serialize.hpp"

#include "level.hpp"
#include "renderer.hpp"

Level loaded_level;

void initDefaultLevel(Level& level, AssetManager& assets) {
    level.environment.sun_direction = glm::normalize(glm::vec3(-1, -1, -0.25)); // use z = -2.25 for matching stonewall skybox
    level.environment.sun_color = 5.0f * glm::vec3(0.941, 0.933, 0.849);
    
    Frustrum frustrum;
    frustrum.aspect_ratio = (float)window_width / (float)window_height;
    level.camera = Camera(frustrum);
    
    /*createEnvironmentFromCubemap(graphics::environment, global_assets,
        { "data/textures/simple_skybox/0006.png", "data/textures/simple_skybox/0002.png",
          "data/textures/simple_skybox/0005.png", "data/textures/simple_skybox/0004.png",
          "data/textures/simple_skybox/0003.png", "data/textures/simple_skybox/0001.png" });*/
    createEnvironmentFromCubemap(level.environment, assets, "data/textures/stonewall_skybox", GL_RGB16F);

    // Load background music
    //auto bg_music = assets.createAudio("data/audio/time.wav");
    //if (bg_music->wav_stream.load(bg_music->handle.c_str()) != SoLoud::SO_NO_ERROR)
    //    std::cout << "Error loading wav\n";
    //bg_music->wav_stream.setLooping(1);                          // Tell SoLoud to loop the sound
    //int handle1 = soloud.play(bg_music->wav_stream);             // Play it
    //soloud.setVolume(handle1, 0.5f);            // Set volume; 1.0f is "normal"
    //soloud.setPan(handle1, -0.2f);              // Set pan; -1 is left, 1 is right
    //soloud.setRelativePlaySpeed(handle1, 1.0f); // Play a bit slower; 1.0f is normal

    level.path = "";
}

bool saveLevel(Level& level, const std::string& path) {
    std::cout << "----------- Writing Level " << path << "----------\n";

    FILE* f = fopen(path.c_str(), "wb");
    if (!f) {
        std::cerr << "Failed to open level path: " << path << " for saving.\n";
        return false;
    }
    writeLevel(level, f);

    fclose(f);
    return true;
}

bool loadLevel(Level& level, AssetManager& assets, const std::string& path) {
    std::cout << "----------- Loading Level " << path << "----------\n";

    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        std::cerr << "Failed to open level path: " << path << " for reading.\n";
        return false;
    }

    readLevel(level, assets, f);
    fclose(f);

    // Update water collision map
    auto& entities = level.entities;
    if (entities.water != NULLID) {
        // @todo make better systems for determining when to update shadow map
        auto water = (WaterEntity*)entities.getEntity(entities.water);
        if (water != nullptr) {
            RenderQueue q;
            createRenderQueue(q, entities);
            bindDrawWaterColliderMap(q, water);
            distanceTransformWaterFbo(water);
        }
        else {
            entities.water = NULLID;
        }
    }

    level.path = path;
    return true;
}
