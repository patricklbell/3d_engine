#ifndef GRAPHICS_HPP
#define GRAPHICS_HPP

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <Camera/core.hpp>

#include "assets.hpp"

class EntityManager;
struct Mesh;
struct Texture;
struct WaterEntity;

extern int    window_width;
extern int    window_height;
extern bool   window_resized;

void framebufferSizeCallback(GLFWwindow *window, int width, int height);
void windowSizeCallback(GLFWwindow* window, int width, int height);

struct Entity;
struct PointLight {
    glm::vec3 position;
    glm::vec3 color;
    float scale, attenuation_linear, attenuation_exp, attenuation_constant;
    PointLight(glm::vec3 in_position, glm::vec3 in_color, float diffuse_intensity, float constant, float linear, float exp) : position(in_position), color(in_color), attenuation_exp(exp), attenuation_linear(linear), attenuation_constant(constant){
        float MaxChannel = fmax(fmax(color.x, color.y), color.z);
        scale = (-linear + sqrtf(linear * linear - 4 * exp * (exp - 256 * MaxChannel * diffuse_intensity))) / (2 * exp);
    }
};

struct Environment {
    Texture* skybox = nullptr;
    Texture* skybox_irradiance = nullptr;
    Texture* skybox_specular = nullptr;
};

void initGraphicsPrimitives(AssetManager &asset_manager);
void drawQuad();
void drawCube();
void drawLineCube();

void updateShadowVP(const Camera &camera);
void initShadowFbo();
void bindDrawShadowMap(const EntityManager &entity_manager, const Camera &camera);

void initBRDFLut(AssetManager& asset_manager);

void initAnimationUbo();

void initWaterColliderFbo();
void bindDrawWaterColliderMap(const EntityManager &entity_manager, WaterEntity *water);
void distanceTransformWaterFbo(WaterEntity* water);

void clearFramebuffer();
void bindHdr();
void drawSkybox(const Texture* skybox, const Camera& camera);
void drawEntitiesHdr(const EntityManager& entity_manager, const Texture* skybox, const Texture* irradiance_map, const Texture* prefiltered_specular_map, const Camera& camera, const bool lightmapping=false);

void bindBackbuffer();
void drawPost(Texture *skybox, const Camera& camera);

void blurBloomFbo();
void initBloomFbo(bool resize=false);

void initHdrFbo(bool resize=false);

void convoluteIrradianceFromCubemap(Texture* in_tex, Texture* out_tex, GLint format = GL_RGB);
void convoluteSpecularFromCubemap(Texture* in_tex, Texture* out_tex, GLint format = GL_RGB);
bool createEnvironmentFromCubemap(Environment& env, AssetManager& asset_manager, const std::array<std::string, FACE_NUM_FACES>& paths, GLint format = GL_RGB);
Texture* createJitter3DTexture(AssetManager& asset_manager, int size, int samples_u, int samples_v);

struct BloomMipInfo {
    glm::vec2 size;
    GLuint texture;
};

constexpr int BLOOM_DOWNSAMPLES = 4;
namespace graphics{
    extern bool do_bloom;
    extern GLuint bloom_fbo;
    extern std::vector<BloomMipInfo> bloom_mip_infos;

    extern GLuint hdr_fbo;
    extern GLuint hdr_buffer;

    extern const std::string shadow_shader_macro;
    extern GLuint shadow_buffer, shadow_fbo;
    extern bool do_shadows;

    extern GLuint water_collider_fbos[2], water_collider_buffers[2];
    extern int water_collider_final_fbo;

    extern const std::string animation_macro;

    extern Mesh quad;
    extern Mesh cube;
    extern Mesh water_grid;

    extern Texture * simplex_gradient;
    extern Texture * simplex_value;

    extern bool do_msaa;
    extern int MSAA_SAMPLES;

    extern Environment environment;
}
    
#endif // GRAPHICS_HPP
