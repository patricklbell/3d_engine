#ifndef ENGINE_GRAPHICS_HPP
#define ENGINE_GRAPHICS_HPP

#include <cstdint>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <camera/core.hpp>

#include "assets.hpp"
#include "entities.hpp"
#include "renderer.hpp"
#include "level.hpp"

extern int    window_width;
extern int    window_height;
extern bool   window_resized;

void framebufferSizeCallback(GLFWwindow* window, int width, int height);
void windowSizeCallback(GLFWwindow* window, int width, int height);

void initGraphics(AssetManager& asset_manager);
void drawQuad();
void drawCube();
void drawLineCube();
void drawTessellatedGrid();

void updateShadowVP(const Camera& camera, glm::vec3 light_direction);
void initShadowFbo();
void writeShadowVpsUbo();

struct Environment;
void computeVolumetrics(const Environment& env, LightQueue& lights, uint64_t frame_i, const Camera& camera);

void initBRDFLut(AssetManager& asset_manager);

void initWaterColliderFbo();
void bindDrawWaterColliderMap(const RenderQueue& q, WaterEntity* water);
void distanceTransformWaterFbo(WaterEntity* water);

void clearFramebuffer();
void bindHdr();

void createRenderQueue(RenderQueue& q, const EntityManager& entity_manager, const bool lightmapping = false);
void createLightQueue(LightQueue& q, const EntityManager& entity_manager);
void addLightsToRenderQueue(RenderQueue& q, LightQueue& lights);
void frustrumCullRenderQueue(RenderQueue& q, const Camera& camera);
void uncullRenderQueue(RenderQueue& q);
void drawRenderQueue(const RenderQueue& q, Environment& env, const Camera& camera);
void drawRenderQueueShadows(const RenderQueue& q);
void drawSkybox(const Texture* skybox, const Camera& camera);

void bindBackbuffer();
void drawPost(const Camera& camera);

void blurBloomFbo(double dt);
void initBloomFbo(bool resize = false);

void initHdrFbo(bool resize = false);

void convoluteIrradianceFromCubemap(Texture* in_tex, Texture* out_tex, GLint format = GL_RGB);
void convoluteSpecularFromCubemap(Texture* in_tex, Texture* out_tex, GLint format = GL_RGB);
bool createEnvironmentFromCubemap(Environment& env, AssetManager& asset_manager, const std::string& path, GLint format = GL_RGB);
Texture* createJitter3DTexture(AssetManager& asset_manager, int size, int samples_u, int samples_v);

void writeFramebufferToTga(std::string path);

struct BloomMipInfo {
    glm::vec2 size;
    GLuint texture;
};

constexpr int BLOOM_DOWNSAMPLES = 4;
constexpr int SHADOW_CASCADE_NUM = 4;

namespace graphics {
    extern bool do_bloom;

    extern bool do_shadows;
    extern const std::string shadow_shader_macro;
    extern float shadow_cascade_distances[SHADOW_CASCADE_NUM];
    extern int shadow_size;
    extern glm::mat4x4 shadow_vps[SHADOW_CASCADE_NUM];

    extern bool do_volumetrics;
    extern const std::string volumetric_shader_macro;

    extern GLuint water_collider_fbos[2], water_collider_buffers[2];
    extern int water_collider_final_fbo;

    extern const std::string animation_macro;

    extern const std::string lights_macro;

    extern Texture* water_noise, * water_normal1, * water_normal2, * water_foam;

    extern bool do_msaa;
    extern int MSAA_SAMPLES;
}

#endif // ENGINE_GRAPHICS_HPP
