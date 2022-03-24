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
    glGenTextures(1, &gb.t_depth);
    glGenTextures(1, &gb.t_final);

    for (unsigned int i = 0 ; i < GBuffer::GBUFFER_NUM_TEXTURES; i++) {
        glBindTexture(GL_TEXTURE_2D, gb.textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, window_width, window_height, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, gb.textures[i], 0);
    }

    // Depth
    glBindTexture(GL_TEXTURE_2D, gb.t_depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH32F_STENCIL8, window_width, window_height, 0, GL_DEPTH_STENCIL,
                  GL_FLOAT_32_UNSIGNED_INT_24_8_REV, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, gb.t_depth, 0);
    
    // Final
    glBindTexture(GL_TEXTURE_2D, gb.t_final);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, window_width, window_height, 0, GL_RGB, GL_FLOAT, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, gb.t_final, 0); 
    
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "Framebuffer failed to initialize, status: 0x%x\n", status);
        return;
    }

    // Restore default FBO
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void clearGBuffer(const GBuffer &gb){
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gb.fbo);
    glDrawBuffer(GL_COLOR_ATTACHMENT4);
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
}

void bindGbuffer(const GBuffer &gb){
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gb.fbo);
    static const GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, drawBuffers);
}

void drawGeometryGbuffer(Entity *entities[ENTITY_COUNT], const Camera &camera){
    // Only the geometry pass updates the depth buffer
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    // Clear depth buffer and color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Note face culling wont work with certain techniques ie grass
    glEnable(GL_CULL_FACE);

    glDisable(GL_BLEND);

    glUseProgram(shader::geometry);
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        auto entity = entities[i];
        if(entity == nullptr)
            continue;

        Mesh *asset = entity->asset;

        auto MVP = camera.projection * camera.view * entity->transform;
        glUniformMatrix4fv(shader::geometry_uniforms.mvp, 1, GL_FALSE, &MVP[0][0]);
        glUniformMatrix4fv(shader::geometry_uniforms.model, 1, GL_FALSE, &entity->transform[0][0]);

        for (int j = 0; j < asset->num_materials; ++j) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, asset->materials[j]->t_diffuse);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, asset->materials[j]->t_normal);

            //bind VAO and draw
            glBindVertexArray(asset->vao);
            glDrawElements(asset->draw_mode, asset->draw_count[j], asset->draw_type, (GLvoid*)(sizeof(GLubyte)*asset->draw_start[j]));
        }
    }

    // Reset gl state
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
}


void bindDeffered(const GBuffer &gb){
    // Draw output to attachment 4 and bind framebuffer textures as uniforms
    glDrawBuffer(GL_COLOR_ATTACHMENT4);
    for (unsigned int i = 0 ; i < GBuffer::GBUFFER_NUM_TEXTURES; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, gb.textures[GBuffer::GBUFFER_TEXTURE_TYPE_POSITION + i]);
    }
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, gb.t_final);
}

// @note quad and sphere are being loaded with unnecessary normal and tangents calculated by assimp
// @note for hdr to be correct color attachment 4 must support hdr
void drawPointLights(const Camera &camera, const GBuffer &gb, const std::vector<PointLight> &point_lights, Mesh *sphere, Mesh *quad){
    glEnable(GL_STENCIL_TEST);
    
    // Setup frame constant uniforms
    glUseProgram(shader::point);
    glUniform2f(shader::point_uniforms.screen_size, window_width, window_height);
    glUniform3fv(shader::point_uniforms.camera_position, 1, &camera.position[0]);

    for (auto &point_light : point_lights) {
        // Disable color/depth write and enable stencil
        glDrawBuffer(GL_FALSE);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glClear(GL_STENCIL_BUFFER_BIT);

        // We need the stencil test to be enabled but we want it
        // to succeed always. Only the depth test matters.
        glStencilFunc(GL_ALWAYS, 0, 0);

        glStencilOpSeparate(GL_BACK, GL_KEEP, GL_INCR_WRAP, GL_KEEP);
        glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_DECR_WRAP, GL_KEEP);

        glUseProgram(shader::null);
        auto MVP = camera.projection * camera.view * glm::scale(glm::translate(glm::mat4x4(), point_light.position), glm::vec3(point_light.scale));
        glUniformMatrix4fv(shader::null_uniforms.mvp, 1, GL_FALSE, &MVP[0][0]);

        // Draw spehere to stencil buffer
        glBindVertexArray(sphere->vao);
        glDrawElements(sphere->draw_mode, sphere->draw_count[0], sphere->draw_type, (GLvoid*)(sizeof(GLubyte)*sphere->draw_start[0]));

        // Draw deffered shading to color attachment 4
        glDrawBuffer(GL_COLOR_ATTACHMENT4);

        // Render deffered light calculations only to stencilled pixels
        glStencilFunc(GL_NOTEQUAL, 0, 0xFF);

        // Blend with other deffered light passes by adding values 
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_ONE, GL_ONE);

        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);

        glUseProgram(shader::point);
        glUniform3fv(shader::point_uniforms.light_color, 1, &point_light.color[0]);
        glUniform3fv(shader::point_uniforms.light_position, 1, &point_light.position[0]);
        glUniform1f(shader::point_uniforms.attenuation_linear, point_light.attenuation_linear);
        glUniform1f(shader::point_uniforms.attenuation_exp, point_light.attenuation_exp);
        glUniform1f(shader::point_uniforms.attenuation_constant, point_light.attenuation_constant);

        glBindVertexArray(sphere->vao);
        glDrawElements(sphere->draw_mode, sphere->draw_count[0], sphere->draw_type, (GLvoid*)0);
        glCullFace(GL_BACK);

        glDisable(GL_BLEND);
    }
    glDisable(GL_STENCIL_TEST);
}

// @todo formalise screen space quad
void drawDirectionalLight(const glm::vec3 &camera_position, Mesh *quad){
    glUseProgram(shader::directional);
    
    // Ignore all depth and culling as rendering to screen quad
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE);

    glUniform2f(shader::directional_uniforms.screen_size, window_width, window_height);
    glUniform3fv(shader::directional_uniforms.light_color, 1, &sun_color[0]);
    glUniform3fv(shader::directional_uniforms.light_direction, 1, &sun_direction[0]);
    glUniform3fv(shader::directional_uniforms.camera_position, 1, &camera_position[0]);

    glBindVertexArray(quad->vao);
    glDrawElements(quad->draw_mode, quad->draw_count[0], quad->draw_type, (GLvoid*)0);
}


// @todo formalise screen space quad
void drawPost(Mesh *quad){
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shader::post);
    
    // Ignore all depth and culling as rendering to screen quad
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    glUniform2f(shader::post_uniforms.screen_size, window_width, window_height);

    glBindVertexArray(quad->vao);
    glDrawElements(quad->draw_mode, quad->draw_count[0], quad->draw_type, (GLvoid*)0);
}

void bindPost(const GBuffer &gb){
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, gb.fbo);
    glReadBuffer(GL_COLOR_ATTACHMENT4);

    glDisable(GL_BLEND);
    glBlitFramebuffer(0, 0, window_width, window_height,
                      0, 0, window_width, window_height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    //glDrawBuffer(GL_COLOR_ATTACHMENT0);
    //glActiveTexture(GL_TEXTURE0);
    //glBindTexture(GL_TEXTURE_2D, gb.t_final);
}
