#include <algorithm>
#include <limits>
#include <stdio.h>
#include <stdlib.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "graphics.hpp"
#include "assets.hpp"
#include "utilities.hpp"
#include "globals.hpp"
#include "shader.hpp"
#include "entities.hpp"

int    window_width;
int    window_height;
bool   window_resized;
namespace graphics{
    GLuint bloom_fbos[2];
    GLuint bloom_buffers[2];
    GLuint hdr_fbo;
    GLuint hdr_buffers[2];
    GLuint shadow_fbo;
    GLuint shadow_buffer;
    glm::mat4x4 shadow_vp;
    Mesh quad;
}
// @hardcoded Cascaded shadow maps are better
static const int SHADOW_SIZE = 2048;

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
    updateCameraView(camera);
    updateCameraProjection(camera);
}

void updateCameraView(Camera &camera){
    camera.view = glm::lookAt(camera.position, camera.target, camera.up);
}

void updateCameraProjection(Camera &camera){
    camera.projection = glm::perspective(glm::radians(45.0f), (float)window_width/(float)window_height, camera.near_plane, camera.far_plane);
}

void updateShadowVP(const Camera &camera){
    // Make shadow's view target the center of the camera frustrum
    glm::vec3 center = camera.position + glm::normalize(camera.target - camera.position)*(camera.near_plane + (camera.far_plane - camera.near_plane)/2);
    const auto shadow_view = glm::lookAt(-sun_direction + center, center, camera.up);

    // 
    // Fit shadow map to camera's frustrum
    //
    const auto inv_VP = glm::inverse(camera.projection * camera.view);
    
    typedef std::numeric_limits<float> lim;
    float min_x = lim::max(), min_y = lim::max(), min_z = lim::max();
    float max_x = lim::min(), max_y = lim::min(), max_z = lim::min();
    for (int x = -1; x < 2; x+=2){
        for (int y = -1; y < 2; y+=2){
            for (int z = -1; z < 2; z+=2){
                const glm::vec4 world_p = inv_VP * glm::vec4(x,y,z,1.0f);
                const glm::vec4 shadow_p = shadow_view * (world_p / world_p.w);
                //printf("shadow position: %f, %f, %f, %f\n", shadow_p.x, shadow_p.y, shadow_p.z, shadow_p.w);
                min_x = std::min(shadow_p.x, min_x);
                min_y = std::min(shadow_p.y, min_y);
                min_z = std::min(shadow_p.z, min_z);
                max_x = std::max(shadow_p.x, max_x);
                max_y = std::max(shadow_p.y, max_y);
                max_z = std::max(shadow_p.z, max_z);
                //printf("x: %f %f y: %f %f z: %f %f\n", min_x, max_x, min_y, max_y, min_z, max_z);
            }
        }
    }
    // @todo Tune this parameter according to the scene
    constexpr float z_mult = 10.0f;
    if (min_z < 0){
        min_z *= z_mult;
    } else{
        min_z /= z_mult;
    } if (max_z < 0){
        max_z /= z_mult;
    } else{
        max_z *= z_mult;
    }

    // Round bounds to reduce artifacts when moving camera
    // @note Relies on shadow map being square
    float max_world_units_per_texel = 100*(glm::tan(glm::radians(45.0f)) * (camera.near_plane+camera.far_plane)) / SHADOW_SIZE;
    //printf("World units per texel %f\n", max_world_units_per_texel);
    min_x = glm::floor(min_x / max_world_units_per_texel) * max_world_units_per_texel;
    max_x = glm::floor(max_x / max_world_units_per_texel) * max_world_units_per_texel;    
    min_y = glm::floor(min_y / max_world_units_per_texel) * max_world_units_per_texel;
    max_y = glm::floor(max_y / max_world_units_per_texel) * max_world_units_per_texel;    
    min_z = glm::floor(min_z / max_world_units_per_texel) * max_world_units_per_texel;
    max_z = glm::floor(max_z / max_world_units_per_texel) * max_world_units_per_texel;

    const auto shadow_projection = glm::ortho(min_x, max_x, min_y, max_y, min_z, max_z);
    //const auto shadow_projection = glm::ortho(50.0f, -50.0f, 50.0f, -50.0f, camera.near_plane, camera.far_plane);
    graphics::shadow_vp = shadow_projection * shadow_view;
}

void createShadowFbo(){
    glGenFramebuffers(1, &graphics::shadow_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, graphics::shadow_fbo);

    glGenTextures(1, &graphics::shadow_buffer);
    glBindTexture(GL_TEXTURE_2D, graphics::shadow_buffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, 
                 SHADOW_SIZE, SHADOW_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor); 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
		 

	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, graphics::shadow_buffer, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
	
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	    fprintf(stderr, "Failed to create shadow fbo.\n");	

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Since shadow buffer wont need to be bound otherwise just combine these operations
void bindDrawShadowMap(const EntityManager &entity_manager, const Camera &camera){
    glBindFramebuffer(GL_FRAMEBUFFER, graphics::shadow_fbo);
    glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);
    
    // Make shadow line up with object by culling front (Peter Panning)
    // @note face culling wont work with certain techniques i.e. grass
    glDisable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    glUseProgram(shader::null_program);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); 

    //glDisable(GL_BLEND);
    glClear(GL_DEPTH_BUFFER_BIT);

    for (int i = 0; i < ENTITY_COUNT; ++i) {
        auto m_e = (MeshEntity*)entity_manager.entities[i];
        if(m_e == nullptr || !(m_e->type & EntityType::MESH_ENTITY) || !m_e->casts_shadow) continue;

        auto mvp = graphics::shadow_vp * createModelMatrix(m_e->position, m_e->rotation, m_e->scale);
        glUniformMatrix4fv(shader::null_uniforms.mvp, 1, GL_FALSE, &mvp[0][0]);

        Mesh *mesh = m_e->mesh;
        for (int j = 0; j < mesh->num_materials; ++j) {
            glBindVertexArray(mesh->vao);
            glDrawElements(mesh->draw_mode, mesh->draw_count[j], mesh->draw_type, (GLvoid*)(sizeof(GLubyte)*mesh->draw_start[j]));
        }
    }
    glCullFace(GL_BACK);

    // Reset bound framebuffer and return to original viewport
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, window_width, window_height);
}

void createBloomFbo(){
    glGenFramebuffers(2, graphics::bloom_fbos);
    glGenTextures(2, graphics::bloom_buffers);
    for (unsigned int i = 0; i < 2; i++){
        glBindFramebuffer(GL_FRAMEBUFFER, graphics::bloom_fbos[i]);
        glBindTexture(GL_TEXTURE_2D, graphics::bloom_buffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, window_width, window_height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // we clamp to the edge as the blur filter would otherwise sample repeated texture values
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, graphics::bloom_buffers[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
            printf("Bloom framebuffer not complete.\n");
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void createHdrFbo(){
    int num_buffers = (int)shader::unified_bloom + 1;
    glGenFramebuffers(1, &graphics::hdr_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, graphics::hdr_fbo);
    glGenTextures(num_buffers, graphics::hdr_buffers);
    for (unsigned int i = 0; i < num_buffers; i++){
        glBindTexture(GL_TEXTURE_2D, graphics::hdr_buffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, window_width, window_height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // attach texture to framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, graphics::hdr_buffers[i], 0);
    }
    // create and attach depth buffer (renderbuffer)
    GLuint rbo_depth;
    glGenRenderbuffers(1, &rbo_depth);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo_depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, window_width, window_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo_depth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
        printf("Hdr framebuffer not complete.\n");
    }
    static const GLuint attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void initGraphicsPrimitives(){
    // @hardcoded
    static const float quad_vertices[] = {
        // positions        // texture Coords
        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
    };
    glGenVertexArrays(1, &graphics::quad.vao);
    GLuint quad_vbo;
    glGenBuffers(1, &quad_vbo);

    glBindVertexArray(graphics::quad.vao);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);

    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), &quad_vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);

	graphics::quad.draw_count = (GLint*)malloc(sizeof(GLint));
	graphics::quad.draw_start = (GLint*)malloc(sizeof(GLint));
    graphics::quad.draw_mode = GL_TRIANGLE_STRIP;
    graphics::quad.draw_type = GL_UNSIGNED_SHORT;

    graphics::quad.draw_start[0] = 0; 
    graphics::quad.draw_count[0] = 4;
}

void drawScreenQuad(){
    glBindVertexArray(graphics::quad.vao);
    glDrawArrays(graphics::quad.draw_mode, graphics::quad.draw_start[0], graphics::quad.draw_count[0]);
}

// Returns the index of the resulting blur buffer in bloom_buffers
int blurBloomFbo(){
    bool horizontal = true, first_iteration = true;
    int amount = 3;
    glUseProgram(shader::gaussian_blur_program);
    for (unsigned int i = 0; i < amount; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, graphics::bloom_fbos[horizontal]); 

        glUniform1i(shader::gaussian_blur_uniforms.horizontal, (int)horizontal);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, first_iteration ? graphics::hdr_buffers[1] : graphics::bloom_buffers[!horizontal]); 

        drawScreenQuad();
        horizontal = !horizontal;
        if (first_iteration) first_iteration = false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0); 
    return (int)!horizontal;
}

void bindHdr(){
    if(shader::unified_bloom){
        glBindFramebuffer(GL_FRAMEBUFFER, graphics::hdr_fbo);
    } else {
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
    }
}

void clearFramebuffer(const glm::vec4 &color){
    glClearColor(color.x, color.y, color.z, color.w);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0.0,0.0,0.0,1.0);
}

void drawUnifiedHdr(const EntityManager &entity_manager, const Camera &camera){
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    // @note face culling wont work with certain techniques i.e. grass
    glEnable(GL_CULL_FACE);

    if(!shader::unified_bloom) glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    else                       glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(shader::unified_programs[shader::unified_bloom]);
    glUniform3fv(shader::unified_uniforms[shader::unified_bloom].sun_color, 1, &sun_color[0]);
    glUniform3fv(shader::unified_uniforms[shader::unified_bloom].sun_direction, 1, &sun_direction[0]);
    glUniform3fv(shader::unified_uniforms[shader::unified_bloom].camera_position, 1, &camera.position[0]);
    
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, graphics::shadow_buffer);

    // @speed already calculated when performing dynamic shadow mapping
    auto vp = camera.projection * camera.view;
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        auto m_e = (MeshEntity*)entity_manager.entities[i];
        if(m_e == nullptr || !(m_e->type & EntityType::MESH_ENTITY))
            continue;

        auto trans = createModelMatrix(m_e->position, m_e->rotation, m_e->scale);
        auto shadow_mvp = graphics::shadow_vp * trans;
        glUniformMatrix4fv(shader::unified_uniforms[shader::unified_bloom].shadow_mvp, 1, GL_FALSE, &shadow_mvp[0][0]);

        auto mvp = vp * trans;
        glUniformMatrix4fv(shader::unified_uniforms[shader::unified_bloom].mvp, 1, GL_FALSE, &mvp[0][0]);
        glUniformMatrix4fv(shader::unified_uniforms[shader::unified_bloom].model, 1, GL_FALSE, &trans[0][0]);

        auto mesh = m_e->mesh;
        for (int j = 0; j < mesh->num_materials; ++j) {
            auto &mat = mesh->materials[j];
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

            // Bind VAO and draw
            glBindVertexArray(mesh->vao);
            glDrawElements(mesh->draw_mode, mesh->draw_count[j], mesh->draw_type, (GLvoid*)(sizeof(GLubyte)*mesh->draw_start[j]));
        }
    }

    // Reset gl state
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
}

void bindBackbuffer(){
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void drawPost(int bloom_buffer_index){
    // Draw screen space quad so clearing is unnecessary
    //glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
    //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDepthMask(GL_FALSE);

    glUseProgram(shader::post_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, graphics::hdr_buffers[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, graphics::bloom_buffers[bloom_buffer_index]);
    drawScreenQuad();
}
