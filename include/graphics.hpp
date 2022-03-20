#ifndef GRAPHIC_H
#define GRAPHIC_H

extern int    window_width;
extern int    window_height;
extern bool   window_resized;
extern Camera camera;

void window_size_callback(GLFWwindow* window, int width, int height){
    if(width != window_width || height != window_height) window_resized = true;
    else                                                 window_resized = false;

    window_width  = width;
    window_height = height;
}

typedef struct Camera {
    enum State { 
        TRACKBALL = 0,
        SHOOTER = 1,
    } state;

    glm::vec3 const up = glm::vec3(0,1,0);
    glm::vec3 position;
    glm::vec3 pivot;
    float distance;
    glm::mat4 view;
    glm::mat4 projection;
} Camera;

void update_camera_view(Camera &camera);
void load_default_camera(Camera &camera);

struct GBuffer {
    enum GBUFFER_TEXTURE_TYPE {
        GBUFFER_TEXTURE_TYPE_POSITION = 0,
        GBUFFER_TEXTURE_TYPE_DIFFUSE  = 1,
        GBUFFER_TEXTURE_TYPE_NORMAL   = 2,
        GBUFFER_NUM_TEXTURES          = 3
    };
    GLuint fbo;
    GLuint textures[GBUFFER_NUM_TEXTURES];
    GLuint depthTexture;
    GLuint finalTexture;
} typedef GBuffer;

GBuffer generate_gbuffer(unsigned int windowWidth, unsigned int windowHeight);

#endif
