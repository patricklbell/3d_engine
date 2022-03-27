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

void bindHdr(){
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
}

void clearHdr(){
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void drawUnifiedHdr(Entity *entities[ENTITY_COUNT], const Camera &camera){
    // Only the geometry pass updates the depth buffer
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    // Clear depth buffer and color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Note face culling wont work with certain techniques ie grass
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glUseProgram(shader::unified_program);
    glUniform3fv(shader::unified_uniforms.sun_color, 1, &sun_color[0]);
    glUniform3fv(shader::unified_uniforms.sun_direction, 1, &sun_direction[0]);
    glUniform3fv(shader::unified_uniforms.camera_position, 1, &camera.position[0]);
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        auto entity = entities[i];
        if(entity == nullptr)
            continue;

        Mesh *asset = entity->asset;

        auto MVP = camera.projection * camera.view * entity->transform;
        glUniformMatrix4fv(shader::unified_uniforms.mvp, 1, GL_FALSE, &MVP[0][0]);
        glUniformMatrix4fv(shader::unified_uniforms.model, 1, GL_FALSE, &entity->transform[0][0]);

        for (int j = 0; j < asset->num_materials; ++j) {
            auto &mat = asset->materials[j];
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mat->t_albedo);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, mat->t_normal);

            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, mat->t_metallic);

            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, mat->t_roughness);

            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D, mat->t_ambient);

            //bind VAO and draw
            glBindVertexArray(asset->vao);
            glDrawElements(asset->draw_mode, asset->draw_count[j], asset->draw_type, (GLvoid*)(sizeof(GLubyte)*asset->draw_start[j]));
        }
    }

    // Reset gl state
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
}
