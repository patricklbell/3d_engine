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

struct PointLight {
    glm::vec3 position;
    glm::vec3 color;
    float scale, attenuation_linear, attenuation_exp, attenuation_constant;
    PointLight(glm::vec3 in_position, glm::vec3 in_color, float diffuse_intensity, float constant, float linear, float exp) : position(in_position), color(in_color), attenuation_exp(exp), attenuation_linear(linear), attenuation_constant(constant){
        float MaxChannel = fmax(fmax(color.x, color.y), color.z);
        scale = (-linear + sqrtf(linear * linear - 4 * exp * (exp - 256 * MaxChannel * diffuse_intensity))) / (2 * exp);
    }
};

struct Camera {
    enum TYPE { 
        TRACKBALL = 0,
        SHOOTER = 1,
    } state;

    const float near_plane = 1.0f, far_plane = 100.0f;
    const glm::vec3 up = glm::vec3(0,1,0);
    glm::vec3 position;
    glm::vec3 target;
    glm::mat4 view;
    glm::mat4 projection;
};

void createDefaultCamera(Camera &camera);
void updateCameraView(Camera &camera);
void updateCameraProjection(Camera &camera);

void initGraphicsPrimitives();
void drawScreenQuad();

void updateShadowVP(const Camera &camera);
void createShadowFbo();
void bindDrawShadowMap(Entity **entities, const Camera &camera);

void clearFramebuffer(const glm::vec4 &color);
void bindHdr();
void drawUnifiedHdr(Entity *entities[ENTITY_COUNT], const Camera &camera);

void bindBackbuffer();
void drawPost(int bloom_buffer_index);

int blurBloomFbo();
void createBloomFbo();
void createHdrFbo();
namespace graphics {
    extern GLuint bloom_fbos[2];
    extern GLuint bloom_buffers[2];
    extern GLuint hdr_fbo;
    extern GLuint hdr_buffers[2];
    extern GLuint shadow_fbo;
    extern GLuint shadow_buffer;
    extern glm::mat4x4 shadow_vp;
}
    
#endif
