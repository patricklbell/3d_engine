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
Camera editor_camera;
Camera level_camera;
Camera game_camera;

namespace graphics {
    bool do_bloom = true;
    GLuint bloom_fbo = {GL_FALSE};
    std::vector<BloomMipInfo> bloom_mip_infos;

    // 
    // HDR
    //
    GLuint hdr_fbo = { GL_FALSE };
    GLuint hdr_buffer = { GL_FALSE };
    GLuint hdr_depth = { GL_FALSE };
    // Used by water pass to write and read from same depth buffer
    GLuint hdr_depth_copy;

    // 
    // SHADOWS
    //
    constexpr int SHADOW_SIZE = 4096;
    constexpr int shadow_num = 4;
    float shadow_cascade_distances[shadow_num];
    // @note make sure to change macro to shadow_num + 1
    const char * shadow_shader_macro = "#define CASCADE_NUM 4\n#define INVOCATIONS 5\n";
    glm::mat4x4 shadow_vps[shadow_num + 1];
    GLuint shadow_fbo, shadow_buffer, shadow_matrices_ubo;

    //
    // WATER PLANE COLLISIONS
    //
    GLuint water_collider_fbos[2] = { GL_FALSE }, water_collider_buffers[2] = { GL_FALSE };
    int water_collider_final_fbo = 0;
    constexpr int WATER_COLLIDER_SIZE = 4096;

    //
    // BONES/ANIMATION
    //
    GLuint bone_matrices_ubo;

    //
    // ASSETS
    //
    Mesh quad, cube, line_cube, water_grid, seaweed;
    Texture * simplex_gradient;
    Texture * simplex_value;

    // @hardcoded
    const int MSAA_SAMPLES = 2;
}

using namespace graphics;


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
    camera.right = glm::vec3(glm::transpose(camera.view)[0]);
    camera.forward = glm::normalize(camera.position - camera.target);
}

void updateCameraProjection(Camera &camera){
    camera.projection = glm::perspective(camera.fov, (float)window_width/(float)window_height, camera.near_plane, camera.far_plane);
}

void updateCameraTarget(Camera& camera, glm::vec3 target) {
    camera.target = target;
    updateCameraView(camera);
    updateShadowVP(camera);
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
    shadow_cascade_distances[0] = camera.far_plane / 50.0f;
    shadow_cascade_distances[1] = camera.far_plane / 25.0f;
    shadow_cascade_distances[2] = camera.far_plane / 10.0f;
    shadow_cascade_distances[3] = camera.far_plane / 2.0f;
    float np = camera.near_plane, fp;
    for(int i = 0; i < shadow_num; i++){
        fp = shadow_cascade_distances[i];
        shadow_vps[i] = getShadowMatrixFromFrustrum(camera, np, fp);
        np = fp;
    }
    shadow_vps[shadow_num] = getShadowMatrixFromFrustrum(camera, np, camera.far_plane);

    // @note if something else binds another ubo to 0 then this will be overwritten
    glBindBuffer(GL_UNIFORM_BUFFER, shadow_matrices_ubo);
    for (size_t i = 0; i < shadow_num + 1; ++i)
    {
        glBufferSubData(GL_UNIFORM_BUFFER, i * sizeof(glm::mat4x4), sizeof(glm::mat4x4), &shadow_vps[i]);
    }
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void initShadowFbo(){
    glGenFramebuffers(1, &shadow_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);

    glGenTextures(1, &shadow_buffer);
    glBindTexture(GL_TEXTURE_2D_ARRAY, shadow_buffer);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F, 
                 SHADOW_SIZE, SHADOW_SIZE, shadow_num+1, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float border_col[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, border_col); 
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);

	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadow_buffer, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
	
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "Failed to create shadow fbo.\n";

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGenBuffers(1, &shadow_matrices_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, shadow_matrices_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4x4) * (shadow_num+1), nullptr, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, shadow_matrices_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}


// Since shadow buffer wont need to be bound otherwise just combine these operations
void bindDrawShadowMap(const EntityManager &entity_manager, const Camera &camera){
    glBindBuffer(GL_UNIFORM_BUFFER, shadow_matrices_ubo);
    for (size_t i = 0; i < shadow_num + 1; ++i)
    {
        glBufferSubData(GL_UNIFORM_BUFFER, i * sizeof(glm::mat4x4), sizeof(glm::mat4x4), &shadow_vps[i]);
    }

    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);
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

    std::vector<AnimatedMeshEntity*> anim_mesh_entities;

    // 
    // Draw normal static mesh entities
    //
    glUseProgram(shader::null.program);
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        if (entity_manager.entities[i] == nullptr) continue;

        auto m_e = reinterpret_cast<MeshEntity*>(entity_manager.entities[i]);
        if (entityInherits(m_e->type, ANIMATED_MESH_ENTITY)) {
            auto a_e = (AnimatedMeshEntity*)m_e;
            if (playing || a_e->draw_animated) {
                anim_mesh_entities.push_back(a_e);
                continue;
            }
        }
        if(!(entityInherits(m_e->type, MESH_ENTITY)) || m_e->mesh == nullptr) continue;

        auto g_model = createModelMatrix(m_e->position, m_e->rotation, m_e->scale);

        auto& mesh = m_e->mesh;
        glBindVertexArray(mesh->vao);
        for (int j = 0; j < mesh->num_meshes; ++j) {
            auto model = g_model * mesh->transforms[j];
            glUniformMatrix4fv(shader::null.uniform("model"), 1, GL_FALSE, &model[0][0]);

            glDrawElements(mesh->draw_mode, mesh->draw_count[j], mesh->draw_type, (GLvoid*)(sizeof(*mesh->indices) * mesh->draw_start[j]));
        }
    }

    // 
    // Draw animated PBR mesh entities, same as static with some extra uniforms
    //
    if (anim_mesh_entities.size() > 0) {
        glUseProgram(shader::animated_null.program);

        for (const auto& a_e : anim_mesh_entities) {
            if (a_e->animesh == nullptr) continue;

            // @note if something else binds another ubo to 1 then this will be overwritten
            // Reloads ubo everytime, this is slow but don't know any other way
            glBindBuffer(GL_UNIFORM_BUFFER, bone_matrices_ubo);
            for (size_t i = 0; i < MAX_BONES; ++i)
            {
                glBufferSubData(GL_UNIFORM_BUFFER, i * sizeof(glm::mat4x4), sizeof(glm::mat4x4), &a_e->final_bone_matrices[i]);
            }
            glBindBuffer(GL_UNIFORM_BUFFER, 1);

            auto model = createModelMatrix(a_e->position, a_e->rotation, a_e->scale);
            glUniformMatrix4fv(shader::animated_null.uniform("model"), 1, GL_FALSE, &model[0][0]);

            auto& mesh = a_e->mesh;
            glBindVertexArray(mesh->vao);
            for (int j = 0; j < mesh->num_meshes; ++j) {
                //auto model = mesh->transforms[j] * g_model; // Shouldn't be needed since this is baked into bone matrices
                // Bind VAO and draw
                glDrawElements(mesh->draw_mode, mesh->draw_count[j], mesh->draw_type, (GLvoid*)(sizeof(*mesh->indices) * mesh->draw_start[j]));
            }
        }
    }

    glCullFace(GL_BACK);
    // Reset bound framebuffer and return to original viewport
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, window_width, window_height);
}

void initAnimationUbo() {
    glGenBuffers(1, &bone_matrices_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, bone_matrices_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4x4) * (MAX_BONES), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, bone_matrices_ubo);
}

void initWaterColliderFbo() {
    glGenFramebuffers(2, water_collider_fbos);
    glGenTextures(2, water_collider_buffers);

    static const GLuint attachments[] = { GL_COLOR_ATTACHMENT0 };
    for (unsigned int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, water_collider_fbos[i]);
        glDrawBuffers(1, attachments);
        glBindTexture(GL_TEXTURE_2D, water_collider_buffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, WATER_COLLIDER_SIZE, WATER_COLLIDER_SIZE, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // we clamp to the edge as the blur filter would otherwise sample repeated texture values
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, water_collider_buffers[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "Water collider framebuffer not complete.\n";
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Since water buffer wont need to be bound otherwise just combine these operations
//void bindDrawWaterColliderMap(const EntityManager &entity_manager, WaterEntity *water){
//    // Create orthographic VP
//    // This makes the horizontal axis of the texture the x dimension
//    float x_len = water->scale[0][0];
//    float z_len = water->scale[2][2];
//    constexpr float height = 0.02;
//    const auto view = glm::lookAt(water->position + glm::vec3(0,2*height,0), water->position, glm::vec3(1,0,0));
//    const auto projection = glm::ortho(z_len, -z_len, -x_len, x_len, height, 3.0f*height);
//    const auto vp = projection * view;
//
//    glBindFramebuffer(GL_FRAMEBUFFER, water_collider_fbos[0]);
//    glViewport(0, 0, WATER_COLLIDER_SIZE, WATER_COLLIDER_SIZE);
//
//    // Render entities only when they intersect water plane
//    glUseProgram(shader::white.program);
//    glDisable(GL_DEPTH_TEST);
//    glDepthMask(GL_TRUE);
//    glDisable(GL_CULL_FACE);
//    
//    glClearColor(0,0,0,1);
//    glClear(GL_COLOR_BUFFER_BIT);
//
//    glUniform1f(shader::white.uniform("height"), 2.0*height);
//    for (int i = 0; i < ENTITY_COUNT; ++i) {
//        auto m_e = reinterpret_cast<MeshEntity*>(entity_manager.entities[i]);
//        if(m_e == nullptr || m_e->type != EntityType::MESH_ENTITY || m_e->mesh == nullptr) continue;
//
//        auto model = createModelMatrix(m_e->position, m_e->rotation, m_e->scale);
//        auto mvp = vp * model;
//        glUniformMatrix4fv(shader::white.uniform("mvp"), 1, GL_FALSE, &mvp[0][0]);
//
//        auto &mesh = m_e->mesh;
//        for (int j = 0; j < mesh->num_materials; ++j) {
//            glBindVertexArray(mesh->vao);
//            glDrawElements(mesh->draw_mode, mesh->draw_count[j], mesh->draw_type, (GLvoid*)(sizeof(GLubyte)*mesh->draw_start[j]));
//        }
//    }
//    // Reset gl state to expected degree
//    glEnable(GL_CULL_FACE);
//    glEnable(GL_DEPTH_TEST);
//    glBindFramebuffer(GL_FRAMEBUFFER, 0);
//    glViewport(0, 0, window_width, window_height);
//    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
//}

void bindDrawWaterColliderMap(const EntityManager& entity_manager, WaterEntity* water) {
    glBindFramebuffer(GL_FRAMEBUFFER, water_collider_fbos[0]);
    glViewport(0, 0, WATER_COLLIDER_SIZE, WATER_COLLIDER_SIZE);

    // Render entities only when they intersect water plane
    glUseProgram(shader::plane_projection.program);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    auto water_grid_scale = water->scale;
    // Remove y component of scale, shouldn't technically be necessary but helps with debugging
    water_grid_scale[1][1] = 1.0f;
    auto inv_water_grid = glm::inverse(createModelMatrix(water->position, glm::quat(), water_grid_scale));
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        auto m_e = reinterpret_cast<MeshEntity*>(entity_manager.entities[i]);
        if (m_e == nullptr || !(entityInherits(m_e->type, MESH_ENTITY)) || m_e->mesh == nullptr) continue;

        auto model = createModelMatrix(m_e->position, m_e->rotation, m_e->scale);
        auto model_inv_water_grid = inv_water_grid * model;
        glUniformMatrix4fv(shader::plane_projection.uniform("model"), 1, GL_FALSE, &model_inv_water_grid[0][0]);

        auto& mesh = m_e->mesh;
        for (int j = 0; j < mesh->num_materials; ++j) {
            glBindVertexArray(mesh->vao);
            glDrawElements(mesh->draw_mode, mesh->draw_count[j], mesh->draw_type, (GLvoid*)(sizeof(GLubyte) * mesh->draw_start[j]));
        }
    }
    // Reset gl state to expected degree
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, window_width, window_height);
}
void distanceTransformWaterFbo(WaterEntity* water) {
    constexpr uint64_t num_steps = nextPowerOf2(WATER_COLLIDER_SIZE, WATER_COLLIDER_SIZE) - 2.0;

    glUseProgram(shader::jfa.program);
    glUniform1f(shader::jfa.uniform("num_steps"), num_steps);
    glUniform2f(shader::jfa.uniform("resolution"), WATER_COLLIDER_SIZE, WATER_COLLIDER_SIZE);

    glViewport(0, 0, WATER_COLLIDER_SIZE, WATER_COLLIDER_SIZE);
    for (int step = 0; step <= num_steps; step++)
    {
        // Last iteration convert jfa to distance transform
        if (step != num_steps) {
            glUniform1f(shader::jfa.uniform("step"), step);
        }
        else {
            glUseProgram(shader::jfa_distance.program);
            glUniform2f(shader::jfa_distance.uniform("dimensions"), water->scale[0][0], water->scale[2][2]);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, water_collider_fbos[(step + 1) % 2]);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, water_collider_buffers[step % 2]);

        drawQuad();
    }


    glBindTexture(GL_TEXTURE_2D, water_collider_buffers[(num_steps + 1) % 2]);
    glGenerateMipmap(GL_TEXTURE_2D);
    water_collider_final_fbo = (num_steps + 1) % 2;

    glUseProgram(shader::gaussian_blur.program);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, water_collider_fbos[(num_steps + i) % 2]);

        glUniform1i(shader::gaussian_blur.uniform("horizontal"), (int)i);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, water_collider_buffers[(num_steps + i + 1) % 2]);

        drawQuad();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, window_width, window_height);
}

void clearBloomFbo() {
    for (auto& mip : bloom_mip_infos) {
        glDeleteTextures(1, &mip.texture);
    }
    bloom_mip_infos.clear();
    glDeleteFramebuffers(1, &bloom_fbo);
    bloom_fbo = GL_FALSE;
}

void initBloomFbo(bool resize) {
    clearBloomFbo();

    glGenFramebuffers(1, &bloom_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, bloom_fbo);

    auto mip_size = glm::vec2(window_width, window_height);
    for (int i = 0; i < BLOOM_DOWNSAMPLES; ++i) {
        auto& mip = bloom_mip_infos.emplace_back();

        mip_size *= 0.5f;
        mip.size = mip_size;

        glGenTextures(1, &mip.texture);
        glBindTexture(GL_TEXTURE_2D, mip.texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F, mip_size.x, mip_size.y, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // we clamp to the edge as the blur filter would otherwise sample repeated texture values
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        if (mip.size.x <= 1 || mip.size.y <= 1)
            break;
    }

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloom_mip_infos[0].texture, 0);
    unsigned int attachments[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, attachments);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Bloom framebuffer not complete.\n";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void initHdrFbo(bool resize) {
    if (!resize || hdr_fbo == GL_FALSE) {
        glGenFramebuffers(1, &hdr_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, hdr_fbo);

        glGenTextures(1, &hdr_buffer);
    }
    else {
        glBindFramebuffer(GL_FRAMEBUFFER, hdr_fbo);
    }

    glBindTexture(GL_TEXTURE_2D, hdr_buffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, window_width, window_height, 0, GL_RGBA, GL_FLOAT, NULL);
    //glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA16F, window_width, window_height, GL_TRUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // attach texture to framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hdr_buffer, 0);

    // @fix if there is no water this copy is unnecessary
    if (!resize || hdr_depth == GL_FALSE) {
        // create and attach depth buffer
        glGenTextures(1, &hdr_depth);
        glGenTextures(1, &hdr_depth_copy);
    }
    glBindTexture(GL_TEXTURE_2D, hdr_depth_copy);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, window_width, window_height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
    //glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_DEPTH24_STENCIL8, window_width, window_height, GL_TRUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, hdr_depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, window_width, window_height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
    //glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_DEPTH24_STENCIL8, window_width, window_height, GL_TRUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, hdr_depth, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Hdr framebuffer not complete.\n";
    }

    static const GLuint attachments[] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, attachments);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void initGraphicsPrimitives(AssetManager& asset_manager) {
    // @hardcoded
    static const float quad_vertices[] = {
        // positions        // texture Coords
        -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
         1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
    };
    glGenVertexArrays(1, &quad.vao);
    GLuint quad_vbo;
    glGenBuffers(1, &quad_vbo);

    glBindVertexArray(quad.vao);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);

    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), &quad_vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);

    quad.draw_count = (GLint*)malloc(sizeof(GLint));
    quad.draw_start = (GLint*)malloc(sizeof(GLint));
    quad.draw_mode = GL_TRIANGLE_STRIP;
    quad.draw_type = GL_UNSIGNED_SHORT;

    quad.draw_start[0] = 0;
    quad.draw_count[0] = 4;

    // @hardcoded
    static const float cube_vertices[] = {
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
    glGenVertexArrays(1, &cube.vao);
    GLuint cube_vbo;
    glGenBuffers(1, &cube_vbo);

    glBindVertexArray(cube.vao);
    glBindBuffer(GL_ARRAY_BUFFER, cube_vbo);

    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), &cube_vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

    glBindVertexArray(0);

    cube.draw_count = (GLint*)malloc(sizeof(GLint));
    cube.draw_start = (GLint*)malloc(sizeof(GLint));
    cube.draw_mode = GL_TRIANGLES;
    cube.draw_type = GL_UNSIGNED_SHORT;

    cube.draw_start[0] = 0;
    cube.draw_count[0] = sizeof(cube_vertices) / (3.0 * sizeof(*cube_vertices));

    // @hardcoded
    static const float line_cube_vertices[] = {
        //0     1.0f, -1.0f, -1.0f,
        //1     1.0f,  1.0f, -1.0f,
        //2    -1.0f,  1.0f, -1.0f,
        //3    -1.0f, -1.0f, -1.0f,
        //4     1.0f, -1.0f,  1.0f,
        //5     1.0f,  1.0f,  1.0f,
        //6    -1.0f, -1.0f,  1.0f,
        //7    -1.0f,  1.0f,  1.0f

             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
    };
    glGenVertexArrays(1, &line_cube.vao);
    GLuint line_cube_vbo;
    glGenBuffers(1, &line_cube_vbo);

    glBindVertexArray(line_cube.vao);
    glBindBuffer(GL_ARRAY_BUFFER, line_cube_vbo);

    glBufferData(GL_ARRAY_BUFFER, sizeof(line_cube_vertices), &line_cube_vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

    glBindVertexArray(0);

    line_cube.draw_count = (GLint*)malloc(sizeof(GLint));
    line_cube.draw_start = (GLint*)malloc(sizeof(GLint));
    line_cube.draw_mode = GL_LINES;
    line_cube.draw_type = GL_UNSIGNED_SHORT;

    line_cube.draw_start[0] = 0;
    line_cube.draw_count[0] = sizeof(line_cube_vertices) / (2.0 * sizeof(*line_cube_vertices));

    asset_manager.loadMeshFile(&water_grid, "data/mesh/water_grid.mesh");
    asset_manager.loadMeshFile(&seaweed, "data/mesh/seaweed.mesh");

    simplex_gradient = asset_manager.createTexture("data/textures/2d_simplex_gradient_seamless.png");
    asset_manager.loadTexture(simplex_gradient, "data/textures/2d_simplex_gradient_seamless.png", GL_RGB);
    simplex_value = asset_manager.createTexture("data/textures/2d_simplex_value_seamless.png");
    asset_manager.loadTexture(simplex_value, "data/textures/2d_simplex_value_seamless.png", GL_RED);

    // Set clear color
    glClearColor(0.0, 0.0, 0.0, 1.0);
}

void drawCube() {
    glBindVertexArray(cube.vao);
    glDrawArrays(cube.draw_mode, cube.draw_start[0], cube.draw_count[0]);
}
void drawLineCube() {
    glBindVertexArray(line_cube.vao);
    glDrawArrays(line_cube.draw_mode, line_cube.draw_start[0], line_cube.draw_count[0]);
}
void drawQuad() {
    glBindVertexArray(quad.vao);
    glDrawArrays(quad.draw_mode, quad.draw_start[0], quad.draw_count[0]);
}

void blurBloomFbo() {
    glBindFramebuffer(GL_FRAMEBUFFER, bloom_fbo);

    // 
    // Progressively downsample screen texture
    //
    glUseProgram(shader::downsample.program);
    glUniform2f(shader::downsample.uniform("resolution"), window_width, window_height);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdr_buffer);

    for (int i = 0; i < bloom_mip_infos.size(); ++i) {
        const auto& mip = bloom_mip_infos[i];

        glViewport(0, 0, mip.size.x, mip.size.y);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mip.texture, 0);

        int is_mip0 = i == bloom_mip_infos.size() - 1;
        glUniform1i(shader::downsample.uniform("is_mip0"), is_mip0);

        drawQuad();

        // Set next iterations properties
        glUniform2fv(shader::downsample.uniform("resolution"), 1, &mip.size[0]);
        glBindTexture(GL_TEXTURE_2D, mip.texture);
    }

    //
    // Upscale and blur progressively back to half resolution
    //
    // Enable additive blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);

    glUseProgram(shader::upsample.program);
    for (int i = bloom_mip_infos.size() - 1; i > 0; i--) {
        const auto& mip = bloom_mip_infos[i];
        const auto& next_mip = bloom_mip_infos[i - 1];

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mip.texture);

        glViewport(0, 0, next_mip.size.x, next_mip.size.y);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, next_mip.texture, 0);

        drawQuad();
    }

    glDisable(GL_BLEND);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, window_width, window_height);
}

void bindHdr() {
    glBindFramebuffer(GL_FRAMEBUFFER, hdr_fbo);
}

void clearFramebuffer() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

//void drawSkybox(const Texture* skybox, const Camera &camera) {
//    glDepthMask(GL_FALSE);
//    glEnable(GL_DEPTH_TEST);
//    // Since skybox shader writes maximum depth of 1.0, for skybox to always be we
//    // need to adjust depth func
//	glDepthFunc(GL_LEQUAL); 
//
//    glDisable(GL_CULL_FACE);
//
//    glUseProgram(shader::skybox.program);
//    auto untranslated_view = glm::mat4(glm::mat3(camera.view));
//    glUniformMatrix4fv(shader::skybox.uniform("view"),       1, GL_FALSE, &untranslated_view[0][0]);
//    glUniformMatrix4fv(shader::skybox.uniform("projection"), 1, GL_FALSE, &camera.projection[0][0]);
//    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->id);
//
//    drawCube();
//
//    // Revert state which is unexpected
//	glDepthFunc(GL_LESS); 
//}

inline static void drawMesh(Shader& s, const Mesh* mesh, const glm::mat4& g_model, const glm::mat4& vp) {
    glBindVertexArray(mesh->vao);
    for (int j = 0; j < mesh->num_meshes; ++j) {
        auto model = mesh->transforms[j] * g_model;
        auto mvp = vp * model;
        glUniformMatrix4fv(s.uniform("mvp"), 1, GL_FALSE, &mvp[0][0]);
        glUniformMatrix4fv(s.uniform("model"), 1, GL_FALSE, &model[0][0]);

        auto& mat = mesh->materials[mesh->material_indices[j]];
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
        glDrawElements(mesh->draw_mode, mesh->draw_count[j], mesh->draw_type, (GLvoid*)(sizeof(*mesh->indices) * mesh->draw_start[j]));
    }
}

void drawUnifiedHdr(const EntityManager& entity_manager, const Texture* skybox, const Camera& camera) {
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    // @note face culling wont work with certain techniques i.e. grass
    glEnable(GL_CULL_FACE);

    // @todo in future entities should probably stored as vectors so you can traverse easily
    std::vector<AnimatedMeshEntity*> anim_mesh_entities;
    std::vector<VegetationEntity*> vegetation_entities;

    auto vp = camera.projection * camera.view;

    // 
    // Draw normal static PBR mesh entities
    //
    glUseProgram(shader::unified.program);
    glUniform3fv(shader::unified.uniform("sun_color"), 1, &sun_color[0]);
    glUniform3fv(shader::unified.uniform("sun_direction"), 1, &sun_direction[0]);
    glUniform3fv(shader::unified.uniform("camera_position"), 1, &camera.position[0]);
    glUniformMatrix4fv(shader::unified.uniform("view"), 1, GL_FALSE, &camera.view[0][0]);

    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D_ARRAY, shadow_buffer);

    glUniform1fv(shader::unified.uniform("shadow_cascade_distances[0]"), shadow_num, shadow_cascade_distances);
    glUniform1f(shader::unified.uniform("far_plane"), camera.far_plane);
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        if (entity_manager.entities[i] == nullptr) continue;

        auto m_e = reinterpret_cast<MeshEntity*>(entity_manager.entities[i]);
        if (entityInherits(m_e->type, ANIMATED_MESH_ENTITY)) {
            auto a_e = (AnimatedMeshEntity*)m_e;
            if (playing || a_e->draw_animated) {
                anim_mesh_entities.push_back(a_e);
                continue;
            }
        }
        if (entityInherits(m_e->type, VEGETATION_ENTITY)) {
            vegetation_entities.push_back((VegetationEntity*)m_e);
            continue;
        }
        if (!(entityInherits(m_e->type, MESH_ENTITY)) || m_e->mesh == nullptr) continue;

        // Material multipliers
        glUniform3fv(shader::unified.uniform("albedo_mult"), 1, &m_e->albedo_mult[0]);
        glUniform1f(shader::unified.uniform("roughness_mult"), m_e->roughness_mult);
        glUniform1f(shader::unified.uniform("metal_mult"),     m_e->metal_mult);
        glUniform1f(shader::unified.uniform("ao_mult"),        m_e->ao_mult);

        auto g_model = createModelMatrix(m_e->position, m_e->rotation, m_e->scale);

        drawMesh(shader::unified, m_e->mesh, g_model, vp);
    }

    // 
    // Draw animated PBR mesh entities, same as static with some extra uniforms
    //
    if (anim_mesh_entities.size() > 0) {
        glUseProgram(shader::animated_unified.program);
        glUniform3fv(shader::animated_unified.uniform("sun_color"), 1, &sun_color[0]);
        glUniform3fv(shader::animated_unified.uniform("sun_direction"), 1, &sun_direction[0]);
        glUniform3fv(shader::animated_unified.uniform("camera_position"), 1, &camera.position[0]);
        glUniformMatrix4fv(shader::animated_unified.uniform("view"), 1, GL_FALSE, &camera.view[0][0]);

        glUniform1fv(shader::animated_unified.uniform("shadow_cascade_distances[0]"), shadow_num, shadow_cascade_distances);
        glUniform1f(shader::animated_unified.uniform("far_plane"), camera.far_plane);
        auto vp = camera.projection * camera.view;
        for (const auto &a_e : anim_mesh_entities) {
            if (a_e->animesh == nullptr) continue;

            // @note if something else binds another ubo to 1 then this will be overwritten
            // Reloads ubo everytime, this is slow but don't know any other way
            glBindBuffer(GL_UNIFORM_BUFFER, bone_matrices_ubo);
            for (size_t i = 0; i < MAX_BONES; ++i)
            {
                glBufferSubData(GL_UNIFORM_BUFFER, i * sizeof(glm::mat4x4), sizeof(glm::mat4x4), &a_e->final_bone_matrices[i]);
            }
            glBindBuffer(GL_UNIFORM_BUFFER, 1);

            // Material multipliers
            glUniform3fv(shader::animated_unified.uniform("albedo_mult"), 1, &a_e->albedo_mult[0]);
            glUniform1f(shader::animated_unified.uniform("roughness_mult"), a_e->roughness_mult);
            glUniform1f(shader::animated_unified.uniform("metal_mult"), a_e->metal_mult);
            glUniform1f(shader::animated_unified.uniform("ao_mult"), a_e->ao_mult);

            auto model = createModelMatrix(a_e->position, a_e->rotation, a_e->scale);
            auto mvp = vp * model;
            glUniformMatrix4fv(shader::animated_unified.uniform("mvp"), 1, GL_FALSE, &mvp[0][0]);
            glUniformMatrix4fv(shader::animated_unified.uniform("model"), 1, GL_FALSE, &model[0][0]);

            auto &mesh = a_e->mesh;
            glBindVertexArray(mesh->vao);
            for (int j = 0; j < mesh->num_meshes; ++j) {
                //auto model = mesh->transforms[j] * g_model; // Shouldn't be needed since this is baked into bone matrices

                auto& mat = mesh->materials[mesh->material_indices[j]];
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
                glDrawElements(mesh->draw_mode, mesh->draw_count[j], mesh->draw_type, (GLvoid*)(sizeof(*mesh->indices) * mesh->draw_start[j]));
            }
        }
    }

    // 
    // Draw vegetation/alpha clipped quads
    // @todo instanced rendering
    //
    if(vegetation_entities.size() > 0) {
        glDisable(GL_CULL_FACE);

        glUseProgram(shader::vegetation.program);
        glUniform1f(shader::vegetation.uniform("time"), glfwGetTime());
        
        // @todo shadow casting on vegetation
        // glActiveTexture(GL_TEXTURE5);
        // glBindTexture(GL_TEXTURE_2D_ARRAY, shadow_buffer);
        // glUniform1fv(shader::unified.uniform("shader")::unified_bloom].shadow_cascade_distances, shadow_num, shadow_cascade_distances);
        // glUniform1f(shader::unified.uniform("shader")::unified_bloom].far_plane, camera.far_plane);

        for (const auto &v_e : vegetation_entities) {
            if(v_e->texture == nullptr) continue;

            auto mvp   = vp * createModelMatrix(v_e->position, v_e->rotation, v_e->scale);
            glUniformMatrix4fv(shader::vegetation.uniform("mvp"), 1, GL_FALSE, &mvp[0][0]);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, v_e->texture->id);

            // @todo Use fewer tri mesh/find some way to make this better
            glBindVertexArray(seaweed.vao);
            glDrawElements(seaweed.draw_mode, seaweed.draw_count[0], seaweed.draw_type, 
                           (GLvoid*)(sizeof(*seaweed.indices)*seaweed.draw_start[0]));
        }

        glEnable(GL_CULL_FACE);
    }

    //drawSkybox(skybox, camera);
    if (entity_manager.water != NULLID) {
        auto water = (WaterEntity*)entity_manager.getEntity(entity_manager.water);
        if (water != nullptr) {
            glUseProgram(       shader::water.program);
            glUniform1fv(       shader::water.uniform("shadow_cascade_distances[0]"), shadow_num, shadow_cascade_distances);
            glUniform1f(        shader::water.uniform("far_plane"), camera.far_plane);
            glUniform3fv(       shader::water.uniform("sun_color"), 1, &sun_color[0]);
            glUniform3fv(       shader::water.uniform("sun_direction"), 1, &sun_direction[0]);
            glUniform3fv(       shader::water.uniform("camera_position"), 1, &camera.position[0]);
            glUniform2f(        shader::water.uniform("resolution"), window_width, window_height);
            glUniform1f(        shader::water.uniform("time"), glfwGetTime());
            glUniformMatrix4fv( shader::water.uniform("view"), 1, GL_FALSE, &camera.view[0][0]);
            // @debug the geometry shader and tesselation
            //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, hdr_buffer);
            // Copy depth buffer
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, hdr_depth_copy);
            glCopyTextureSubImage2D(hdr_depth_copy, 0, 0, 0, 0, 0, window_width, window_height);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, simplex_gradient->id);
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, simplex_value->id);
            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D, water_collider_buffers[water_collider_final_fbo]);
    
            auto model = createModelMatrix(water->position, glm::quat(), water->scale);
            auto mvp = vp * model;
            glUniformMatrix4fv(shader::water.uniform("mvp"), 1, GL_FALSE, &mvp[0][0]);
            glUniformMatrix4fv(shader::water.uniform("model"), 1, GL_FALSE, &model[0][0]);
            glUniform4fv(shader::water.uniform("shallow_color"), 1, &water->shallow_color[0]);
            glUniform4fv(shader::water.uniform("deep_color"), 1, &water->deep_color[0]);
            glUniform4fv(shader::water.uniform("foam_color"), 1, &water->foam_color[0]);

            glBindVertexArray(water_grid.vao);
            glDrawElements(water_grid.draw_mode, water_grid.draw_count[0], water_grid.draw_type, 
                           (GLvoid*)(sizeof(*water_grid.indices)*water_grid.draw_start[0]));

            //glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glDepthFunc(GL_LESS);
        }
    }
}

void bindBackbuffer(){
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void drawPost(Texture *skybox, const Camera &camera){
    // Draw screen space quad so clearing is unnecessary
    //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);

    auto& post = shader::post[(int)do_bloom];
    glUseProgram(post.program);

    glUniform2f(post.uniform("resolution"), window_width, window_height);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdr_buffer);

    if (do_bloom) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, bloom_mip_infos[0].texture);
    }

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, hdr_depth);

    // @note view uniform badly named as it is only for the skybox so must be untranslated
    auto untranslated_view = glm::mat4(glm::mat3(camera.view));
    auto inverse_projection_untranslated_view = glm::inverse(camera.projection * untranslated_view);
    glUniformMatrix4fv(post.uniform("inverse_projection_untranslated_view"), 1, GL_FALSE, &inverse_projection_untranslated_view[0][0]);
    //glUniformMatrix4fv(post.uniform("projection"), 1, GL_FALSE, & camera.projection[0][0]);
    //glUniform1f(post.uniform("tan_half_fov"), glm::tan(camera.fov / 2.0f));

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->id);

    drawQuad();
}
