#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "graphics.hpp"

void load_default_camera(Camera &camera){
    camera.state = Camera::State::TRACKBALL;
    camera.position = glm::normalize(glm::vec3(1, 1, 1));
    camera.pivot = glm::vec3(0,0,0);
    camera.distance = 6*glm::length(camera.position - camera.pivot);
    camera.view = glm::lookat(camera.position * camera.distance, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0) );
    camera.projection = glm::perspective(glm::radians(45.0f), (float)window_width/(float)window_height, 0.1f, 100.0f);

}
void update_camera_view(Camera &camera){
    camera.view = glm::lookAt(camera.position * camera.distance, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0) );
}
void update_camera_projection(Camera &camera){
    camera.projection = glm::perspective(glm::radians(45.0f), (float)window_width/(float)window_height, 0.1f, 100.0f);
}

GBuffer generate_gbuffer(){
    GBuffer gb;
    // Create the FBO
    glGenFramebuffers(1, &gb.fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gb.fbo);

    // Create the gbuffer textures
    glGenTextures(GBuffer::GBUFFER_NUM_TEXTURES, gb.textures);
    glGenTextures(1, &gb.depthTexture);
    glGenTextures(1, &gb.finalTexture);

    for (unsigned int i = 0 ; i < GBuffer::GBUFFER_NUM_TEXTURES ; i++) {
        glBindTexture(GL_TEXTURE_2D, gb.textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, window_width, window_height, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, gb.textures[i], 0);
    }

    // depth
    glBindTexture(GL_TEXTURE_2D, gb.depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH32F_STENCIL8, window_width, window_height, 0, GL_DEPTH_STENCIL,
                  GL_FLOAT_32_UNSIGNED_INT_24_8_REV, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, gb.depthTexture, 0);
    
    // final
    glBindTexture(GL_TEXTURE_2D, gb.finalTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, window_width, window_height, 0, GL_RGB, GL_FLOAT, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, gb.finalTexture, 0); 
    
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        printf("status: 0x%x\n", status);
        throw std::runtime_error("Framebuffer failed to generate");
    }

    // restore default FBO
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    return gb;
}


