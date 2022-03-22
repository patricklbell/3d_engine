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
    GLuint depthTexture;
    GLuint finalTexture;
} typedef GBuffer;

void createGBuffer(GBuffer &gb);
void bindGbuffer(GBuffer &gb);
void clearGBuffer(GBuffer &gb);

void drawGeometryGbuffer(Entity *entities[ENTITY_COUNT], const Camera &camera);

void bindDeffered(GBuffer &gb);
void drawPost(const glm::vec3 &camera_position, Asset *quad);
void drawGbufferToBackbuffer(GBuffer &gb);

#endif
