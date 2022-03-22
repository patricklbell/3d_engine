#include <stdio.h>
#include <stdlib.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "graphics.hpp"
#include "utilities.hpp"
#include "globals.hpp"
#include "shader.hpp"

int    window_width;
int    window_height;
bool   window_resized;
Material *default_material;

void windowSizeCallback(GLFWwindow* window, int width, int height){
    if(width != window_width || height != window_height) window_resized = true;
    else                                                 window_resized = false;

    window_width  = width;
    window_height = height;
}

void createDefaultCamera(Camera &camera){
    camera.state = Camera::TYPE::TRACKBALL;
    camera.position = glm::vec3(3,3,3);
    camera.target = glm::vec3(0,0,0);
    updateCameraProjection(camera);
    updateCameraView(camera);
}
void updateCameraView(Camera &camera){
    camera.view = glm::lookAt(camera.position, camera.target, camera.up);
}
void updateCameraProjection(Camera &camera){
    camera.projection = glm::perspective(glm::radians(45.0f), (float)window_width/(float)window_height, 0.1f, 100.0f);
}

void createGBuffer(GBuffer &gb){
    // Create the FBO
    glGenFramebuffers(1, &gb.fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gb.fbo);

    // Create the gbuffer textures
    glGenTextures(GBuffer::GBUFFER_NUM_TEXTURES, gb.textures);
    glGenTextures(1, &gb.depthTexture);
    glGenTextures(1, &gb.finalTexture);

    for (unsigned int i = 0 ; i < GBuffer::GBUFFER_NUM_TEXTURES; i++) {
        glBindTexture(GL_TEXTURE_2D, gb.textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, window_width, window_height, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, gb.textures[i], 0);
    }

    // Depth
    glBindTexture(GL_TEXTURE_2D, gb.depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH32F_STENCIL8, window_width, window_height, 0, GL_DEPTH_STENCIL,
                  GL_FLOAT_32_UNSIGNED_INT_24_8_REV, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, gb.depthTexture, 0);
    
    // Final
    glBindTexture(GL_TEXTURE_2D, gb.finalTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, window_width, window_height, 0, GL_RGB, GL_FLOAT, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, gb.finalTexture, 0); 
    
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        printf("Framebuffer failed to initialize, status: 0x%x\n", status);
        throw;
    }

    // Restore default FBO
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void clearGBuffer(GBuffer &gb){
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gb.fbo);
    glDrawBuffer(GL_COLOR_ATTACHMENT4);
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
}

void bindGbuffer(GBuffer &gb){
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gb.fbo);
    static const GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, drawBuffers);
}

void drawGeometryGbuffer(Entity *entities[ENTITY_COUNT], const Camera &camera){
    // Only the geometry pass updates the depth buffer
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    // Note face culling wont work with certain techniques ie grass
    glEnable(GL_CULL_FACE);

    for (int i = 0; i < ENTITY_COUNT; ++i) {
        auto entity = entities[i];
        if(entity == nullptr)
            continue;

        Asset *asset = entity->asset;

        glUseProgram(asset->program_id);

        auto MVP = camera.projection * camera.view * entity->transform;

        glUniformMatrix4fv(shader::geometry_uniforms.mvp, 1, GL_FALSE, &MVP[0][0]);
        glUniformMatrix4fv(shader::geometry_uniforms.model, 1, GL_FALSE, &entity->transform[0][0]);

        for (int j = 0; j < asset->num_meshes; ++j) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, asset->materials[j]->t_diffuse);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, asset->materials[j]->t_normal);

            //bind VAO and draw
            glBindVertexArray(asset->vao);
            glDrawElements(asset->draw_mode, asset->draw_count[j], asset->draw_type, (GLvoid*)(sizeof(GLubyte) * asset->draw_start[j]));
        }
    }
}


void bindDeffered(GBuffer &gb){
    // Draw output to attachment 4 and bind framebuffer textures as uniforms
    glDrawBuffer(GL_COLOR_ATTACHMENT4);
    for (unsigned int i = 0 ; i < GBuffer::GBUFFER_NUM_TEXTURES; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, gb.textures[GBuffer::GBUFFER_TEXTURE_TYPE_POSITION + i]);
    }
}

// &todo formalise screen space quad
void drawPost(const glm::vec3 &camera_position, Asset *quad){
    glUseProgram(shader::post);
    
    // Ignore all depth and culling as rendering to screen quad
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE);

    glUniform2f(shader::post_uniforms.screen_size, window_width, window_height);
    glUniform3fv(shader::post_uniforms.light_color, 1, &sun_color[0]);
    glUniform3fv(shader::post_uniforms.light_direction, 1, &sun_direction[0]);
    glUniform3fv(shader::post_uniforms.camera_position, 1, &camera_position[0]);

    glBindVertexArray(quad->vao);
    glDrawElements(quad->draw_mode, quad->draw_count[0], quad->draw_type, (GLvoid*)0);
}

void drawGbufferToBackbuffer(GBuffer &gb){
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, gb.fbo);
    glReadBuffer(GL_COLOR_ATTACHMENT4);

    glDisable(GL_BLEND);
    glBlitFramebuffer(0, 0, window_width, window_height, 0, 0, window_width, window_height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
}
