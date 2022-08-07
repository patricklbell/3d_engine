#include <algorithm>
#include <limits>
#include <stdio.h>
#include <stdlib.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "glm/detail/type_mat.hpp"
#include "graphics.hpp"
#include "assets.hpp"
#include "utilities.hpp"
#include "globals.hpp"
#include "shader.hpp"
#include "entities.hpp"
#include "editor.hpp"

int    window_width;
int    window_height;
bool   window_resized;
namespace graphics{
    GLuint bloom_fbos[2];
    GLuint bloom_buffers[2];
    GLuint hdr_fbo;
    GLuint hdr_buffers[2];
    GLuint hdr_depth;
    const int shadow_num = 4;
    float shadow_cascade_distances[shadow_num];
    const char * shadow_macro = "#define CASCADE_NUM 4\n";
    // @note make sure to change macro to shadow_num + 1
    const char * shadow_invocation_macro = "#define INVOCATIONS 5\n";
    GLuint shadow_fbo;
    GLuint shadow_buffer;
    GLuint shadow_matrices_ubo;
    glm::mat4x4 shadow_vps[shadow_num + 1];
    Mesh quad;
    Mesh cube;
    Mesh grid;
}
// @hardcoded
static const int SHADOW_SIZE = 4096;

void windowSizeCallback(GLFWwindow* window, int width, int height){
    if(width != window_width || height != window_height) window_resized = true;

    window_width  = width;
    window_height = height;
}
void framebufferSizeCallback(GLFWwindow *window, int width, int height){}

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

glm::mat4x4 getShadowMatrixFromFrustrum(const Camera &camera, float near_plane, float far_plane){
    // Make shadow's view target the center of the camera frustrum by averaging frustrum corners
    // maybe you can just calculate from view direction and near and far
    const auto camera_projection_alt = glm::perspective(glm::radians(45.0f), (float)window_width/(float)window_height, near_plane, far_plane);
    const auto inv_VP = glm::inverse(camera_projection_alt * camera.view);

    std::vector<glm::vec4> frustrum;
    auto center = glm::vec3(0.0);
    for (int x = -1; x < 2; x+=2){
        for (int y = -1; y < 2; y+=2){
            for (int z = -1; z < 2; z+=2){
                auto p = inv_VP * glm::vec4(x,y,z,1.0f);
                auto wp = p / p.w;
                center += glm::vec3(wp);
                frustrum.push_back(wp);
            }
        }
    }
    center /= frustrum.size();
 
    const auto shadow_view = glm::lookAt(-sun_direction + center, center, camera.up);
    using lim = std::numeric_limits<float>;
    float min_x = lim::max(), min_y = lim::max(), min_z = lim::max();
    float max_x = lim::min(), max_y = lim::min(), max_z = lim::min();
    for(const auto &wp: frustrum){
        const glm::vec4 shadow_p = shadow_view * wp;
        //printf("shadow position: %f, %f, %f, %f\n", shadow_p.x, shadow_p.y, shadow_p.z, shadow_p.w);
        min_x = std::min(shadow_p.x, min_x);
        min_y = std::min(shadow_p.y, min_y);
        min_z = std::min(shadow_p.z, min_z);
        max_x = std::max(shadow_p.x, max_x);
        max_y = std::max(shadow_p.y, max_y);
        max_z = std::max(shadow_p.z, max_z);
    }
    //printf("x: %f %f y: %f %f z: %f %f\n", min_x, max_x, min_y, max_y, min_z, max_z);

    // @todo Tune this parameter according to the scene
    constexpr float z_mult = 10.0f;
    if (min_z < 0){
        min_z *= z_mult;
    } else {
        min_z /= z_mult;
    } if (max_z < 0){
        max_z /= z_mult;
    } else {
        max_z *= z_mult;
    }

    // Round bounds to reduce artifacts when moving camera
    // @note Relies on shadow map being square
    //float max_world_units_per_texel = 100*(glm::tan(glm::radians(45.0f)) * (near_plane+far_plane)) / SHADOW_SIZE;
    //printf("World units per texel %f\n", max_world_units_per_texel);
    //min_x = glm::floor(min_x / max_world_units_per_texel) * max_world_units_per_texel;
    //max_x = glm::floor(max_x / max_world_units_per_texel) * max_world_units_per_texel;    
    //min_y = glm::floor(min_y / max_world_units_per_texel) * max_world_units_per_texel;
    //max_y = glm::floor(max_y / max_world_units_per_texel) * max_world_units_per_texel;    
    //min_z = glm::floor(min_z / max_world_units_per_texel) * max_world_units_per_texel;
    //max_z = glm::floor(max_z / max_world_units_per_texel) * max_world_units_per_texel;

    const auto shadow_projection = glm::ortho(min_x, max_x, min_y, max_y, min_z, max_z);
    //const auto shadow_projection = glm::ortho(50.0f, -50.0f, 50.0f, -50.0f, camera.near_plane, camera.far_plane);
    return shadow_projection * shadow_view;
}

void updateShadowVP(const Camera &camera){
    graphics::shadow_cascade_distances[0] = camera.far_plane / 50.0f;
    graphics::shadow_cascade_distances[1] = camera.far_plane / 25.0f;
    graphics::shadow_cascade_distances[2] = camera.far_plane / 10.0f;
    graphics::shadow_cascade_distances[3] = camera.far_plane / 2.0f;
    float np = camera.near_plane, fp;
    for(int i = 0; i < graphics::shadow_num; i++){
        fp = graphics::shadow_cascade_distances[i];
        graphics::shadow_vps[i] = getShadowMatrixFromFrustrum(camera, np, fp);
        np = fp;
    }
    graphics::shadow_vps[graphics::shadow_num] = getShadowMatrixFromFrustrum(camera, np, camera.far_plane);

    // @note if something else binds another ubo to 0 then this will be overwritten
    glBindBuffer(GL_UNIFORM_BUFFER, graphics::shadow_matrices_ubo);
    for (size_t i = 0; i < graphics::shadow_num + 1; ++i)
    {
        glBufferSubData(GL_UNIFORM_BUFFER, i * sizeof(glm::mat4x4), sizeof(glm::mat4x4), &graphics::shadow_vps[i]);
    }
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void initShadowFbo(){
    glGenFramebuffers(1, &graphics::shadow_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, graphics::shadow_fbo);

    glGenTextures(1, &graphics::shadow_buffer);
    glBindTexture(GL_TEXTURE_2D_ARRAY, graphics::shadow_buffer);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F, 
                 SHADOW_SIZE, SHADOW_SIZE, graphics::shadow_num+1, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float border_col[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, border_col); 
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_TEXTURE_2D_ARRAY, graphics::shadow_buffer, 0);

	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, graphics::shadow_buffer, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
	
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "Failed to create shadow fbo.\n";

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGenBuffers(1, &graphics::shadow_matrices_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, graphics::shadow_matrices_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4x4) * (graphics::shadow_num+1), nullptr, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, graphics::shadow_matrices_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

// Since shadow buffer wont need to be bound otherwise just combine these operations
void bindDrawShadowMap(const EntityManager &entity_manager, const Camera &camera){
    glBindBuffer(GL_UNIFORM_BUFFER, graphics::shadow_matrices_ubo);
    for (size_t i = 0; i < graphics::shadow_num + 1; ++i)
    {
        glBufferSubData(GL_UNIFORM_BUFFER, i * sizeof(glm::mat4x4), sizeof(glm::mat4x4), &graphics::shadow_vps[i]);
    }

    glUseProgram(shader::null_program);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, graphics::shadow_fbo);
    glViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);
    
    // Make shadow line up with object by culling front (Peter Panning)
    // @note face culling wont work with certain techniques i.e. grass
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glDisable(GL_CULL_FACE);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
	//glDepthFunc(GL_LESS); 
    
    //glDisable(GL_BLEND);
    glClear(GL_DEPTH_BUFFER_BIT);

    for (int i = 0; i < ENTITY_COUNT; ++i) {
        auto m_e = reinterpret_cast<MeshEntity*>(entity_manager.entities[i]);
        if(m_e == nullptr || m_e->type != EntityType::MESH_ENTITY || m_e->mesh == nullptr || !m_e->casts_shadow) continue;

        auto model = createModelMatrix(m_e->position, m_e->rotation, m_e->scale);
        glUniformMatrix4fv(shader::null_uniforms.model, 1, GL_FALSE, &model[0][0]);

        auto &mesh = m_e->mesh;
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

void initBloomFbo(bool resize){
    if(!resize){
        glGenFramebuffers(2, graphics::bloom_fbos);
        glGenTextures(2, graphics::bloom_buffers);
    }
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
            std::cerr << "Bloom framebuffer not complete.\n";
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void initHdrFbo(bool resize){
    int num_buffers = (int)shader::unified_bloom + 1;
    if(!resize){
        glGenFramebuffers(1, &graphics::hdr_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, graphics::hdr_fbo);

        glGenTextures(num_buffers, graphics::hdr_buffers);
    }
    for (unsigned int i = 0; i < num_buffers; i++){
        glBindTexture(GL_TEXTURE_2D, graphics::hdr_buffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, window_width, window_height, 0, GL_RGBA, GL_FLOAT, NULL);
        //glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA16F, window_width, window_height, GL_TRUE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // attach texture to framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, graphics::hdr_buffers[i], 0);
    }
    if(!resize){
        // create and attach depth buffer
        glGenTextures(1, &graphics::hdr_depth);
    }
    glBindTexture(GL_TEXTURE_2D, graphics::hdr_depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, window_width, window_height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
    //glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_DEPTH24_STENCIL8, window_width, window_height, GL_TRUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, graphics::hdr_depth, 0);  

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
        std::cerr << "Hdr framebuffer not complete.\n";
    }
    if(shader::unified_bloom){
        static const GLuint attachments[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        glDrawBuffers(2, attachments);
    } else {
        static const GLuint attachments[] = { GL_COLOR_ATTACHMENT0 };
        glDrawBuffers(2, attachments);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void initGraphicsPrimitives(AssetManager &asset_manager) {
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

    // @hardcoded
    static const float cube_vertices[] = {
//        -1.0f,-1.0f,-1.0f, // triangle 1 : begin
//        -1.0f,-1.0f, 1.0f,
//        -1.0f, 1.0f, 1.0f, // triangle 1 : end
//        1.0f, 1.0f,-1.0f,  // triangle 2 : begin
//        -1.0f,-1.0f,-1.0f,
//        -1.0f, 1.0f,-1.0f, // triangle 2 : end
//        1.0f,-1.0f, 1.0f,
//        -1.0f,-1.0f,-1.0f,
//        1.0f,-1.0f,-1.0f,
//        1.0f, 1.0f,-1.0f,
//        1.0f,-1.0f,-1.0f,
//        -1.0f,-1.0f,-1.0f,
//        -1.0f,-1.0f,-1.0f,
//        -1.0f, 1.0f, 1.0f,
//        -1.0f, 1.0f,-1.0f,
//        1.0f,-1.0f, 1.0f,
//        -1.0f,-1.0f, 1.0f,
//        -1.0f,-1.0f,-1.0f,
//        -1.0f, 1.0f, 1.0f,
//        -1.0f,-1.0f, 1.0f,
//        1.0f,-1.0f, 1.0f,
//        1.0f, 1.0f, 1.0f,
//        1.0f,-1.0f,-1.0f,
//        1.0f, 1.0f,-1.0f,
//        1.0f,-1.0f,-1.0f,
//        1.0f, 1.0f, 1.0f,
//        1.0f,-1.0f, 1.0f,
//        1.0f, 1.0f, 1.0f,
//        1.0f, 1.0f,-1.0f,
//        -1.0f, 1.0f,-1.0f,
//        1.0f, 1.0f, 1.0f,
//        -1.0f, 1.0f,-1.0f,
//        -1.0f, 1.0f, 1.0f,
//        1.0f, 1.0f, 1.0f,
//        -1.0f, 1.0f, 1.0f,
//        1.0f,-1.0f, 1.0f
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };
    glGenVertexArrays(1, &graphics::cube.vao);
    GLuint cube_vbo;
    glGenBuffers(1, &cube_vbo);

    glBindVertexArray(graphics::cube.vao);
    glBindBuffer(GL_ARRAY_BUFFER, cube_vbo);

    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), &cube_vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

    glBindVertexArray(0);

	graphics::cube.draw_count = (GLint*)malloc(sizeof(GLint));
	graphics::cube.draw_start = (GLint*)malloc(sizeof(GLint));
    graphics::cube.draw_mode = GL_TRIANGLES;
    graphics::cube.draw_type = GL_UNSIGNED_SHORT;

    graphics::cube.draw_start[0] = 0; 
    graphics::cube.draw_count[0] = sizeof(cube_vertices) / (3.0 * sizeof(*cube_vertices));

    asset_manager.loadMeshFile(&graphics::grid, "data/models/grid.mesh");
}
void drawCube(){
    glBindVertexArray(graphics::cube.vao);
    glDrawArrays(graphics::cube.draw_mode, graphics::cube.draw_start[0], graphics::cube.draw_count[0]);
}
void drawQuad(){
    glBindVertexArray(graphics::quad.vao);
    glDrawArrays(graphics::quad.draw_mode, graphics::quad.draw_start[0],  graphics::quad.draw_count[0]);
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

        drawQuad();
        horizontal = !horizontal;
        if (first_iteration) first_iteration = false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0); 
    return (int)!horizontal;
}

void bindHdr(){
    glBindFramebuffer(GL_FRAMEBUFFER, graphics::hdr_fbo);
}

void clearFramebuffer(const glm::vec4 &color){
    glClearColor(color.x, color.y, color.z, color.w);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void drawSkybox(const Texture* skybox, const Camera &camera) {
    glDepthMask(GL_FALSE);
    glEnable(GL_DEPTH_TEST);
    // Since skybox shader writes maximum depth of 1.0, for skybox to always be we
    // need to adjust depth func
	glDepthFunc(GL_LEQUAL); 

    glDisable(GL_CULL_FACE);

    glUseProgram(shader::skybox_program);
    auto untranslated_view = glm::mat4(glm::mat3(camera.view));
    glUniformMatrix4fv(shader::skybox_uniforms.view,       1, GL_FALSE, &untranslated_view[0][0]);
    glUniformMatrix4fv(shader::skybox_uniforms.projection, 1, GL_FALSE, &camera.projection[0][0]);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->id);

    drawCube();

    // Revert state which is unexpected
	glDepthFunc(GL_LESS); 
}

void drawUnifiedHdr(const EntityManager &entity_manager, const Texture* skybox, const Camera &camera){
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    // @note face culling wont work with certain techniques i.e. grass
    glEnable(GL_CULL_FACE);

    glUseProgram(shader::unified_programs[shader::unified_bloom]);
    glUniform3fv(shader::unified_uniforms[shader::unified_bloom].sun_color, 1, &sun_color[0]);
    glUniform3fv(shader::unified_uniforms[shader::unified_bloom].sun_direction, 1, &sun_direction[0]);
    glUniform3fv(shader::unified_uniforms[shader::unified_bloom].camera_position, 1, &camera.position[0]);
    glUniformMatrix4fv(shader::unified_uniforms[shader::unified_bloom].view, 1, GL_FALSE, &camera.view[0][0]);
    
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D_ARRAY, graphics::shadow_buffer);

    // @ In general we could store lists of needed types with the entity_manager
    std::vector<int> water_ids;
    bool draw_water = false;

    glUniform1fv(shader::unified_uniforms[shader::unified_bloom].shadow_cascade_distances, graphics::shadow_num, graphics::shadow_cascade_distances);
    glUniform1f(shader::unified_uniforms[shader::unified_bloom].far_plane, camera.far_plane);

    auto vp = camera.projection * camera.view;
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        auto m_e = reinterpret_cast<MeshEntity*>(entity_manager.entities[i]);
        if(m_e == nullptr) continue;
        if(m_e->type == EntityType::WATER_ENTITY){
            water_ids.push_back(i); 
            draw_water = true;
            continue;
        }
        if(m_e->type != EntityType::MESH_ENTITY || m_e->mesh == nullptr) continue;

        auto model = createModelMatrix(m_e->position, m_e->rotation, m_e->scale);
        auto mvp = vp * model;
        glUniformMatrix4fv(shader::unified_uniforms[shader::unified_bloom].mvp, 1, GL_FALSE, &mvp[0][0]);
        glUniformMatrix4fv(shader::unified_uniforms[shader::unified_bloom].model, 1, GL_FALSE, &model[0][0]);

        auto &mesh = m_e->mesh;
        for (int j = 0; j < mesh->num_materials; ++j) {
            auto &mat = mesh->materials[j];
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mat.t_albedo->id);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, mat.t_normal->id);

            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, mat.t_metallic->id);

            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, mat.t_roughness->id);

            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D, mat.t_ambient->id);

            // Bind VAO and draw
            glBindVertexArray(mesh->vao);
            glDrawElements(mesh->draw_mode, mesh->draw_count[j], mesh->draw_type, (GLvoid*)(sizeof(GLubyte)*mesh->draw_start[j]));
        }
    }

    drawSkybox(skybox, camera);

    if(draw_water){
        glDepthMask(GL_FALSE);

        glUseProgram(       shader::water_programs[shader::unified_bloom]);
        glUniform1fv(       shader::water_uniforms[shader::unified_bloom].shadow_cascade_distances, graphics::shadow_num, graphics::shadow_cascade_distances);
        glUniform1f(        shader::water_uniforms[shader::unified_bloom].far_plane, camera.far_plane);
        glUniform3fv(       shader::water_uniforms[shader::unified_bloom].sun_color, 1, &sun_color[0]);
        glUniform3fv(       shader::water_uniforms[shader::unified_bloom].sun_direction, 1, &sun_direction[0]);
        glUniform3fv(       shader::water_uniforms[shader::unified_bloom].camera_position, 1, &camera.position[0]);
        glUniform2f(        shader::water_uniforms[shader::unified_bloom].resolution, window_width, window_height);
        glUniform1f(        shader::water_uniforms[shader::unified_bloom].time, glfwGetTime());
        glUniformMatrix4fv( shader::water_uniforms[shader::unified_bloom].view, 1, GL_FALSE, &camera.view[0][0]);

        // @debug the geometry shader and tesselation
        //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, graphics::hdr_buffers[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, graphics::hdr_depth);
    
        // @note Sort by depth
        for(auto &id : water_ids){
            auto e = (WaterEntity*)entity_manager.entities[id];
            auto model = createModelMatrix(e->position, glm::quat(), e->scale);
            auto mvp = vp * model;
            glUniformMatrix4fv(shader::water_uniforms[shader::unified_bloom].mvp, 1, GL_FALSE, &mvp[0][0]);
            glUniformMatrix4fv(shader::water_uniforms[shader::unified_bloom].model, 1, GL_FALSE, &model[0][0]);
            glUniform4fv(shader::water_uniforms[shader::unified_bloom].shallow_color, 1, &e->shallow_color[0]);
            glUniform4fv(shader::water_uniforms[shader::unified_bloom].deep_color, 1, &e->deep_color[0]);
            glUniform4fv(shader::water_uniforms[shader::unified_bloom].foam_color, 1, &e->foam_color[0]);

            glBindVertexArray(graphics::grid.vao);
            glDrawElements(graphics::grid.draw_mode, graphics::grid.draw_count[0], graphics::grid.draw_type, (GLvoid*)(sizeof(GLubyte)*graphics::grid.draw_start[0]));
        }
        //glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}

void bindBackbuffer(){
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void drawPost(int bloom_buffer_index=0){
    // Draw screen space quad so clearing is unnecessary
    //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);

    glUseProgram(shader::post_program[(int)shader::unified_bloom]);

    glUniform2f(shader::post_uniforms[shader::unified_bloom].resolution, window_width, window_height);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, graphics::hdr_buffers[0]);
    if(shader::unified_bloom){
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, graphics::bloom_buffers[bloom_buffer_index]);
    }
    drawQuad();
}
