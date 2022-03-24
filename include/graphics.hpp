#ifndef GRAPHIC_HPP
#define GRAPHIC_HPP

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "utilities.hpp"
#include "globals.hpp"

extern int    window_width;
extern int    window_height;
extern bool   window_resized;

void windowSizeCallback(GLFWwindow* window, int width, int height);

typedef struct PointLight {
    glm::vec3 position;
    glm::vec3 color;
    float scale, attenuation_linear, attenuation_exp, attenuation_constant;
    PointLight(glm::vec3 in_position, glm::vec3 in_color, float diffuse_intensity, float constant, float linear, float exp) : position(in_position), color(in_color), attenuation_exp(exp), attenuation_linear(linear), attenuation_constant(constant){
        float MaxChannel = fmax(fmax(color.x, color.y), color.z);
        scale = (-linear + sqrtf(linear * linear - 4 * exp * (exp - 256 * MaxChannel * diffuse_intensity))) / (2 * exp);
    }
} PointLight;

typedef struct Camera {
    enum TYPE { 
        TRACKBALL = 0,
        SHOOTER = 1,
    } state;

    glm::vec3 const up = glm::vec3(0,1,0);
    glm::vec3 position;
    glm::vec3 target;
    glm::mat4 view;
    glm::mat4 projection;
} Camera;

void createDefaultCamera(Camera &camera);
void updateCameraView(Camera &camera);
void updateCameraProjection(Camera &camera);

struct GBuffer {
    enum GBUFFER_TEXTURE_TYPE {
        GBUFFER_TEXTURE_TYPE_POSITION = 0,
        GBUFFER_TEXTURE_TYPE_DIFFUSE,
        GBUFFER_TEXTURE_TYPE_NORMAL,
        GBUFFER_NUM_TEXTURES,
    };
    GLuint fbo;
    GLuint textures[GBUFFER_NUM_TEXTURES];
    GLuint t_depth;
    GLuint t_final;
} typedef GBuffer;

void createGBuffer(GBuffer &gb);
void bindGbuffer(const GBuffer &gb);
void clearGBuffer(const GBuffer &gb);

void drawGeometryGbuffer(Entity *entities[ENTITY_COUNT], const Camera &camera);

void bindDeffered(const GBuffer &gb);
void drawPointLights(const Camera &camera, const GBuffer &gb, const std::vector<PointLight> &point_lights, Mesh *sphere, Mesh *quad);
void drawDirectionalLight(const glm::vec3 &camera_position, Mesh *quad);
void drawPost(Mesh *quad);
void bindPost(const GBuffer &gb);

#endif
