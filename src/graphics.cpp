#include <algorithm>
#include <limits>
#include <stdio.h>
#include <stdlib.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/detail/type_mat.hpp>

#include <camera/core.hpp>
#include <shader/globals.hpp>

#include "graphics.hpp"
#include "assets.hpp"
#include "utilities.hpp"
#include "globals.hpp"
#include "entities.hpp"
#include "editor.hpp"
#include "game_behaviour.hpp"

int    window_width;
int    window_height;
bool   window_resized;

namespace graphics {
    bool do_bloom = true;
    GLuint bloom_fbo = {GL_FALSE};
    std::vector<BloomMipInfo> bloom_mip_infos;

    // 
    // HDR
    //
    GLuint hdr_fbo = { GL_FALSE };
    GLuint hdr_fbo_resolve_multisample = { GL_FALSE };
    GLuint hdr_buffer = { GL_FALSE };
    GLuint hdr_buffer_resolve_multisample = { GL_FALSE };
    GLuint hdr_depth = { GL_FALSE };
    // Used by water pass to write and read from same depth buffer
    // and resolved to by msaa
    GLuint hdr_depth_copy;

    // 
    // SHADOWS
    //
    int shadow_size = 4096;
    float shadow_cascade_distances[SHADOW_CASCADE_NUM];
    // @note make sure to change macro to SHADOW_CASCADE_NUM + 1
    const std::string shadow_shader_macro = "#define CASCADE_NUM " + std::to_string(SHADOW_CASCADE_NUM) + "\n";
    glm::mat4x4 shadow_vps[SHADOW_CASCADE_NUM];
    GLuint shadow_fbo, shadow_buffer, shadow_matrices_ubo;
    bool do_shadows = true;
    constexpr int JITTER_SIZE = 16;
    Texture* jitter_texture;

    //
    // Volumetric Fog
    //
    bool do_volumetrics = true;
    const glm::ivec3 VOLUMETRIC_RESOLUTION{ 160, 90, 128 };
    constexpr int NUM_BLUE_NOISE_TEXTURES = 16;
    // For now use the same texture for all PM
    std::array<Texture*, NUM_BLUE_NOISE_TEXTURES> blue_noise_textures; 
    constexpr int NUM_TEMPORAL_VOLUMES = 2;
    GLuint temporal_integration_volume[NUM_TEMPORAL_VOLUMES]; // Stores the scattering/transmission for the current and previous frame
    GLuint accumulated_volumetric_volume; // Stores the accumulated result of ray marching the integration volume for the current frame
    const glm::ivec3 VOLUMETRIC_LOCAL_SIZE{ 8, 8, 1 }; // Size of work groups for compute shaders
    const std::string volumetric_shader_macro = "#define LOCAL_SIZE_X " + std::to_string(VOLUMETRIC_LOCAL_SIZE.x) +
                                              "\n#define LOCAL_SIZE_Y " + std::to_string(VOLUMETRIC_LOCAL_SIZE.y) +
                                              "\n#define LOCAL_SIZE_Z " + std::to_string(VOLUMETRIC_LOCAL_SIZE.z) + "\n"; // @todo
    // global fog settings, @todo local fog volumes etc.
    FogProperties global_fog_properties;

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
    const std::string animation_macro = "#define MAX_BONES " + std::to_string(MAX_BONES) +
                                      "\n#define MAX_BONE_WEIGHTS " + std::to_string(MAX_BONE_WEIGHTS) + "\n";

    //
    // ASSETS
    //
    Mesh quad, cube, line_cube, water_grid;
    Texture* simplex_gradient;
    Texture* simplex_value;
    Texture* brdf_lut;

    // 
    // MSAA
    //
    bool do_msaa = false;
    int MSAA_SAMPLES = 4;

    //
    // Environment
    //
    Environment environment;

    //
    // Wind
    // @todo 
    glm::vec3 wind_direction = glm::normalize(glm::vec3(0.5, -0.1, 0.6));
    float wind_strength = 3.0;

    //
    // Autmatic exposure (eye correction)
    //
    const glm::vec3 LUMA_MULT = glm::vec3(0.299, 0.587, 0.114);
    float exposure = 1.0;
    double luma_t = -1.0;

    GLuint shared_uniforms_ubo = GL_FALSE;
}

using namespace graphics;

void windowSizeCallback(GLFWwindow* window, int width, int height) {
    if (width != window_width || height != window_height) window_resized = true;

    window_width = width;
    window_height = height;
}
void framebufferSizeCallback(GLFWwindow* window, int width, int height) {}

static void initBoneMatricesUbo() {
    glGenBuffers(1, &bone_matrices_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, bone_matrices_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4x4) * MAX_BONES, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 2, bone_matrices_ubo);
}

static void writeBoneMatricesUbo(const std::array<glm::mat4, MAX_BONES>& final_bone_matrices) {
    // @note if something else binds another ubo to 1 then this will be overwritten
    // Reloads ubo everytime, this is slow but don't know any other way
    glBindBuffer(GL_UNIFORM_BUFFER, bone_matrices_ubo);
    for (size_t i = 0; i < MAX_BONES; ++i)
    {
        glBufferSubData(GL_UNIFORM_BUFFER, i * sizeof(glm::mat4x4), sizeof(glm::mat4x4), &final_bone_matrices[i]);
    }
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

static glm::mat4x4 getShadowMatrixFromFrustrum(const Camera& camera, double near_plane, double far_plane) {
    // Make shadow's view target the center of the camera frustrum by averaging frustrum corners
    // maybe you can just calculate from view direction and near and far
    const auto camera_projection_alt = glm::perspective((double)camera.frustrum.fov, (double)camera.frustrum.aspect_ratio, near_plane, far_plane);
    const auto inv_VP = glm::inverse(camera_projection_alt * glm::dmat4(camera.view));

    std::vector<glm::vec4> frustrum;
    auto center = glm::dvec3(0.0);
    for (int x = -1; x < 2; x += 2) {
        for (int y = -1; y < 2; y += 2) {
            for (int z = -1; z < 2; z += 2) {
                auto p = inv_VP * glm::vec4(x, y, z, 1.0f);
                auto wp = p / p.w;
                center += glm::vec3(wp);
                frustrum.push_back(wp);
            }
        }
    }
    center /= frustrum.size();

    const auto shadow_view = glm::lookAt(-glm::dvec3(sun_direction) + center, center, glm::dvec3(camera.up));
    using lim = std::numeric_limits<float>;
    double min_x = lim::max(), min_y = lim::max(), min_z = lim::max();
    double max_x = lim::min(), max_y = lim::min(), max_z = lim::min();
    for (const auto& wp : frustrum) {
        const glm::dvec4 shadow_p = shadow_view * wp;
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
    if (min_z < 0) {
        min_z *= z_mult;
    }
    else {
        min_z /= z_mult;
    } if (max_z < 0) {
        max_z /= z_mult;
    }
    else {
        max_z *= z_mult;
    }

    // Round bounds to reduce artifacts when moving camera
    // @note Relies on shadow map being square
    //float max_world_units_per_texel = (glm::tan(glm::radians(45.0f)) * (near_plane+far_plane)) / shadow_size;
    ////printf("World units per texel %f\n", max_world_units_per_texel);
    //min_x = glm::floor(min_x / max_world_units_per_texel) * max_world_units_per_texel;
    //max_x = glm::floor(max_x / max_world_units_per_texel) * max_world_units_per_texel;    
    //min_y = glm::floor(min_y / max_world_units_per_texel) * max_world_units_per_texel;
    //max_y = glm::floor(max_y / max_world_units_per_texel) * max_world_units_per_texel;    
    //min_z = glm::floor(min_z / max_world_units_per_texel) * max_world_units_per_texel;
    //max_z = glm::floor(max_z / max_world_units_per_texel) * max_world_units_per_texel;

    const auto shadow_projection = glm::ortho(min_x, max_x, min_y, max_y, min_z, max_z);
    //const auto shadow_projection = glm::ortho(50.0f, -50.0f, 50.0f, -50.0f, camera.near_plane, camera.frustrum.far_plane);
    return shadow_projection * shadow_view;
}

static void initShadowVpsUbo() {
    glGenBuffers(1, &shadow_matrices_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, shadow_matrices_ubo);
    glBufferData(GL_UNIFORM_BUFFER, 64 * SHADOW_CASCADE_NUM + 16 * SHADOW_CASCADE_NUM, nullptr, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, shadow_matrices_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void writeShadowVpsUbo() {
    // @note if something else binds another ubo to 0 then this will be overwritten
    glBindBuffer(GL_UNIFORM_BUFFER, shadow_matrices_ubo);
    for (size_t i = 0; i < SHADOW_CASCADE_NUM; ++i) {
        glBufferSubData(GL_UNIFORM_BUFFER, i * sizeof(glm::mat4x4), sizeof(glm::mat4x4), &shadow_vps[i]);
    }
    for (size_t i = 0; i < SHADOW_CASCADE_NUM; ++i) {
        glBufferSubData(GL_UNIFORM_BUFFER, SHADOW_CASCADE_NUM * 64 + i * 16, 4, &shadow_cascade_distances[i]);
    }
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void updateShadowVP(const Camera& camera) {
    const double& cnp = camera.frustrum.near_plane, & cfp = camera.frustrum.far_plane * 0.75;

    // Hardcoded csm distances
    // Unity two map split - 0.25f
    // Unity four map split - 0.067f, 0.2f, 0.467f
    /*shadow_cascade_distances[0] = cfp * 0.067;
    shadow_cascade_distances[1] = cfp * 0.2;
    shadow_cascade_distances[2] = cfp * 0.467;
    shadow_cascade_distances[3] = cfp;*/

    double snp = cnp, sfp;
    for (int i = 0; i < SHADOW_CASCADE_NUM; i++) {
        const double p = (double)(i + 1) / (double)SHADOW_CASCADE_NUM;

        // Simple linear formula
        sfp = cnp + (cfp - cnp) * p;
        // More complicated formula which accounts for wasted area from orthographic 
        // projection being fitted to perspective -> 0 as we get further away
        // https://developer.download.nvidia.com/SDK/10.5/opengl/src/cascaded_shadow_maps/doc/cascaded_shadow_maps.pdf
        //constexpr float l = 1.02; // How much to correct for this error
        //sfp = l*cnp*pow(cfp / cnp, p) + (1 - l)*(cnp + p*(cfp - cnp));
        // Use @hardcoded distances, doesn't work when cascade num changes
        //sfp = shadow_cascade_distances[i];

        shadow_vps[i] = getShadowMatrixFromFrustrum(camera, snp, sfp);

        shadow_cascade_distances[i] = sfp;
        snp = sfp;
    }

    writeShadowVpsUbo();
}

void initShadowFbo() {
    glGenFramebuffers(1, &shadow_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);

    glGenTextures(1, &shadow_buffer);
    glBindTexture(GL_TEXTURE_2D_ARRAY, shadow_buffer);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT16,
        shadow_size, shadow_size, SHADOW_CASCADE_NUM + 1, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NULL);
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

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "Failed to create shadow fbo.\n";

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    initShadowVpsUbo();
}

// Precomputes brdf geometry function for different roughnesses and directions
void initBRDFLut(AssetManager& asset_manager) {
    brdf_lut = asset_manager.createTexture("brdf_lut");
    brdf_lut->resolution = glm::ivec2(512, 512); // @hardcoded

    GLuint FBO, RBO;
    glGenFramebuffers(1, &FBO);
    glGenRenderbuffers(1, &RBO);

    // Create brdf_lut
    glGenTextures(1, &brdf_lut->id);
    glBindTexture(GL_TEXTURE_2D, brdf_lut->id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, brdf_lut->resolution.x, brdf_lut->resolution.y, 0, GL_RG, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, FBO);
    glBindRenderbuffer(GL_RENDERBUFFER, RBO);

    glUseProgram(Shaders::generate_brdf_lut.program());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdf_lut->id, 0);
    glViewport(0, 0, brdf_lut->resolution.x, brdf_lut->resolution.y);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawQuad();
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

void bindDrawWaterColliderMap(const EntityManager& entity_manager, WaterEntity* water) {
    glBindFramebuffer(GL_FRAMEBUFFER, water_collider_fbos[0]);
    glViewport(0, 0, WATER_COLLIDER_SIZE, WATER_COLLIDER_SIZE);

    // Render entities only when they intersect water plane
    glUseProgram(Shaders::plane_projection.program());
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
        if (m_e == nullptr || !entityInherits(m_e->type, MESH_ENTITY) || entityInherits(m_e->type, ANIMATED_MESH_ENTITY) || m_e->mesh == nullptr) continue;

        auto model = createModelMatrix(m_e->position, m_e->rotation, m_e->scale);
        auto model_inv_water_grid = inv_water_grid * model;
        glUniformMatrix4fv(Shaders::plane_projection.uniform("model"), 1, GL_FALSE, &model_inv_water_grid[0][0]);

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

    glUseProgram(Shaders::jump_flood.program());
    glUniform1f(Shaders::jump_flood.uniform("num_steps"), num_steps);
    glUniform2f(Shaders::jump_flood.uniform("resolution"), WATER_COLLIDER_SIZE, WATER_COLLIDER_SIZE);

    glViewport(0, 0, WATER_COLLIDER_SIZE, WATER_COLLIDER_SIZE);
    for (int step = 0; step <= num_steps; step++)
    {
        // Last iteration convert jfa to distance transform
        if (step != num_steps) {
            glUniform1f(Shaders::jump_flood.uniform("step"), step);
        }
        else {
            glUseProgram(Shaders::jfa_to_distance.program());
            glUniform2f(Shaders::jfa_to_distance.uniform("dimensions"), water->scale[0][0], water->scale[2][2]);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, water_collider_fbos[(step + 1) % 2]);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, water_collider_buffers[step % 2]);

        drawQuad();
    }


    glBindTexture(GL_TEXTURE_2D, water_collider_buffers[(num_steps + 1) % 2]);
    glGenerateMipmap(GL_TEXTURE_2D);
    water_collider_final_fbo = (num_steps + 1) % 2;

    glUseProgram(Shaders::gaussian_blur.program());
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, water_collider_fbos[(num_steps + i) % 2]);

        glUniform1i(Shaders::gaussian_blur.uniform("horizontal"), (int)i);

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

        glGenTextures(1, &hdr_buffer);
    }
    if (do_msaa && (!resize || hdr_fbo_resolve_multisample == GL_FALSE)) {
        glGenFramebuffers(1, &hdr_fbo_resolve_multisample);
        glGenTextures(1, &hdr_buffer_resolve_multisample);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, hdr_fbo);

    if (do_msaa) {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, hdr_buffer);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, MSAA_SAMPLES, GL_RGBA16F, window_width, window_height, GL_TRUE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, hdr_buffer, 0);
    }
    else {
        glBindTexture(GL_TEXTURE_2D, hdr_buffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, window_width, window_height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hdr_buffer, 0);
    }

    // Initialize depth textures if necessary
    if (!resize || hdr_depth == GL_FALSE) {
        // create and attach depth buffer
        glGenTextures(1, &hdr_depth);
        glGenTextures(1, &hdr_depth_copy);
    }

    // Create actual depth texture, multisampled if necessary
    if (do_msaa) {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, hdr_depth);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, MSAA_SAMPLES, GL_DEPTH24_STENCIL8, window_width, window_height, GL_TRUE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, hdr_depth, 0);
    }
    else {
        glBindTexture(GL_TEXTURE_2D, hdr_depth);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, window_width, window_height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, hdr_depth, 0);
    }

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "Hdr framebuffer not complete, error code: " << glCheckFramebufferStatus(GL_FRAMEBUFFER) << ".\n";

    static const GLuint attachments[] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, attachments);

    // Create copy of depth texture which is sampled by water shader and/or msaa post processing
    // @fix if there is no water (and not msaa) this texture is unnecessary (and might be unnecessary anyway)
    glBindTexture(GL_TEXTURE_2D, hdr_depth_copy);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, window_width, window_height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Since we perform post processing on screen texture we need an intermediate fbo to resolve to
    if (do_msaa) {
        glBindFramebuffer(GL_FRAMEBUFFER, hdr_fbo_resolve_multisample);

        glBindTexture(GL_TEXTURE_2D, hdr_depth_copy); // @note Might be redundant
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, hdr_depth_copy, 0);

        glBindTexture(GL_TEXTURE_2D, hdr_buffer_resolve_multisample);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, window_width, window_height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hdr_buffer_resolve_multisample, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "Hdr resolve framebuffer not complete, error code: " << glCheckFramebufferStatus(GL_FRAMEBUFFER) << ".\n";
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void initSharedUbo() {
    glGenBuffers(1, &shared_uniforms_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, shared_uniforms_ubo);
    glBufferData(GL_UNIFORM_BUFFER, 192, NULL, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, shared_uniforms_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

static void writeSharedUbo(const Camera& camera) {
    // @note if something else binds another ubo to 0 then this will be overwritten
    glBindBuffer(GL_UNIFORM_BUFFER, shared_uniforms_ubo);
    int offset = 0;
    glBufferSubData(GL_UNIFORM_BUFFER, offset, 64, &camera.view[0][0]); offset += 4 * 16;
    glm::mat4 vp = camera.projection * camera.view;
    glBufferSubData(GL_UNIFORM_BUFFER, offset, 64, &vp[0][0]); offset += 4 * 16;
    glBufferSubData(GL_UNIFORM_BUFFER, offset, 16, &sun_direction[0]); offset += 16;
    glBufferSubData(GL_UNIFORM_BUFFER, offset, 16, &sun_color[0]); offset += 16;
    glBufferSubData(GL_UNIFORM_BUFFER, offset, 16, &camera.position[0]); offset += 12;
    float time = glfwGetTime();
    glBufferSubData(GL_UNIFORM_BUFFER, offset, 4, &time); offset += 4;
    glm::ivec2 window_size(window_width, window_height);
    glBufferSubData(GL_UNIFORM_BUFFER, offset, 8, &window_size[0]); offset += 8;
    glBufferSubData(GL_UNIFORM_BUFFER, offset, 4, &camera.frustrum.far_plane); offset += 4;
    glBufferSubData(GL_UNIFORM_BUFFER, offset, 4, &exposure); offset += 4;
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void initVolumetrics() {
    glGenTextures(1, &accumulated_volumetric_volume);
    glBindTexture(GL_TEXTURE_3D, accumulated_volumetric_volume);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, VOLUMETRIC_RESOLUTION.x, VOLUMETRIC_RESOLUTION.y, VOLUMETRIC_RESOLUTION.z, 0, GL_RGBA, GL_HALF_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // We don't want to volumetrics which we haven't calculated
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glGenTextures(NUM_TEMPORAL_VOLUMES, temporal_integration_volume);
    for (int i = 0; i < NUM_TEMPORAL_VOLUMES; ++i) {
        glBindTexture(GL_TEXTURE_3D, temporal_integration_volume[i]);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, VOLUMETRIC_RESOLUTION.x, VOLUMETRIC_RESOLUTION.y, VOLUMETRIC_RESOLUTION.z, 0, GL_RGBA, GL_HALF_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // We don't want to volumetrics which we haven't calculated
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }
}

void computeVolumetrics(uint64_t frame_i, const Camera& camera) {
    // @todo integrate with draw entities so we don't send ubo,
    // in future it would be better to have a system for write ubos easily and checking if something has changed
    writeSharedUbo(camera);

    if (do_shadows) {
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D_ARRAY, shadow_buffer);

        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_3D, jitter_texture->id);
    }

    // Convert fog volumes, for now just a global fog, into camera fitted color&density 3D texture
    auto& vi = Shaders::volumetric_integration;
    glUseProgram(vi.program());

    if (playing) {
        glUniform1f(vi.uniform("anisotropy"), Game::fog_properties.anisotropy);
        glUniform1f(vi.uniform("density"), Game::fog_properties.density);
        glUniform1f(vi.uniform("noise_scale"), Game::fog_properties.noise_scale);
        glUniform1f(vi.uniform("noise_amount"), Game::fog_properties.noise_amount);
    }
    else {
        glUniform1f(vi.uniform("anisotropy"), global_fog_properties.anisotropy);
        glUniform1f(vi.uniform("density"), global_fog_properties.density);
        glUniform1f(vi.uniform("noise_scale"), global_fog_properties.noise_scale);
        glUniform1f(vi.uniform("noise_amount"), global_fog_properties.noise_amount);
    }
    glUniform1i(vi.uniform("do_accumulation"), frame_i != 0);
    glUniform3iv(vi.uniform("vol_size"), 1, &VOLUMETRIC_RESOLUTION[0]);
    glUniform3fv(vi.uniform("wind_direction"), 1, &wind_direction[0]);
    glUniform1f(vi.uniform("wind_strength"), wind_strength);

    const auto inv_vp = glm::inverse(camera.projection * camera.view);
    glUniformMatrix4fv(vi.uniform("inv_vp"), 1, GL_FALSE, &inv_vp[0][0]);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, blue_noise_textures[frame_i % NUM_BLUE_NOISE_TEXTURES]->id);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, temporal_integration_volume[(frame_i + 1) % NUM_TEMPORAL_VOLUMES]);

    glBindImageTexture(0, temporal_integration_volume[frame_i % NUM_TEMPORAL_VOLUMES], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

    glm::ivec3 compute_size = glm::ceil(glm::fvec3(VOLUMETRIC_RESOLUTION) / glm::fvec3(VOLUMETRIC_LOCAL_SIZE));
    glDispatchCompute(compute_size.x, compute_size.y, compute_size.z);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT); // @todo do other rendering steps while waiting

    // Raymarch through fog volume to accumulate scattering and transmission
    auto& vr = Shaders::volumetric_raymarch;
    glUseProgram(vr.program());

    glUniform3iv(vr.uniform("vol_size"), 1, &VOLUMETRIC_RESOLUTION[0]);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, temporal_integration_volume[frame_i % NUM_TEMPORAL_VOLUMES]);

    glBindImageTexture(0, accumulated_volumetric_volume, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);

    glDispatchCompute(compute_size.x, compute_size.y, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT); // @todo do other rendering steps while waiting
}

void initGraphics(AssetManager& asset_manager) {
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

    asset_manager.loadMeshAssimp(&water_grid, "data/models/water_grid.obj");
    simplex_gradient = asset_manager.createTexture("data/textures/2d_simplex_gradient_seamless.png");
    asset_manager.loadTexture(simplex_gradient, "data/textures/2d_simplex_gradient_seamless.png", GL_RGB, GL_REPEAT);
    simplex_value = asset_manager.createTexture("data/textures/2d_simplex_value_seamless.png");
    asset_manager.loadTexture(simplex_value, "data/textures/2d_simplex_value_seamless.png", GL_RED, GL_REPEAT);

    // Create per-pixel jitter lookup textures
    //createJitter3DTexture(jitter_lookup_64, JITTER_SIZE, 8, 8);	// 8 'estimation' samples, 64 total samples
    jitter_texture = createJitter3DTexture(asset_manager, JITTER_SIZE, 4, 8);	// 4 'estimation' samples, 32 total samples

    // Load blue noise textures
    for (int i = 0; i < NUM_BLUE_NOISE_TEXTURES; ++i) {
        std::string p = "data/textures/blue_noise/LDR_LLL1_" + std::to_string(i) + ".png";
        blue_noise_textures[i] = asset_manager.createTexture(p);
        asset_manager.loadTexture(blue_noise_textures[i], p, GL_RGB, GL_REPEAT);
    }

    // Set clear color
    glClearColor(0.0, 0.0, 0.0, 1.0);

    initVolumetrics();
    initShadowFbo();
    initSharedUbo();
    initBoneMatricesUbo();
    initWaterColliderFbo();
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

// https://bruop.github.io/exposure/, and https://knarkowicz.wordpress.com/2016/01/09/automatic-exposure/
// S is the Sensor sensitivity, K is the reflected-light meter calibration constant, 
// q is the lens and vignetting attentuation
static double calculateCameraLuma(double L_avg) {
    constexpr double S = 100, K = 12.5, q = 0.65;
    return 78.0 / (q * S) * L_avg * (S / K);
}

double calculateExposureFromLuma(double L) {
    constexpr double L_min = 0.4, L_max = 1.5, L_ec = 0.1;
    return 0.18 / (glm::clamp(L, L_min, L_max) - L_ec);
}

void blurBloomFbo(double dt) {
    glBindFramebuffer(GL_FRAMEBUFFER, bloom_fbo);

    // 
    // Progressively downsample screen texture
    //
    glUseProgram(Shaders::downsample.program());
    glUniform2f(Shaders::downsample.uniform("resolution"), window_width, window_height);

    glActiveTexture(GL_TEXTURE0);
    if(do_msaa) glBindTexture(GL_TEXTURE_2D, hdr_buffer_resolve_multisample);
    else        glBindTexture(GL_TEXTURE_2D, hdr_buffer);

    for (int i = 0; i < bloom_mip_infos.size(); ++i) {
        const auto& mip = bloom_mip_infos[i];

        glViewport(0, 0, mip.size.x, mip.size.y);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mip.texture, 0);

        int is_mip0 = i == bloom_mip_infos.size() - 1;
        glUniform1i(Shaders::downsample.uniform("is_mip0"), is_mip0);

        drawQuad();

        // Set next iterations properties
        glUniform2fv(Shaders::downsample.uniform("resolution"), 1, &mip.size[0]);
        glBindTexture(GL_TEXTURE_2D, mip.texture);

        // Extract luma from furthest downsample
        if (i == bloom_mip_infos.size() - 1) {
            glGenerateMipmap(GL_TEXTURE_2D);
            glm::fvec3 avg_color{ 1.0 };
            GLint levels = glm::floor(log2(glm::max(mip.size.x, mip.size.y))); // Make enough mip levels to downsample to single pixel
            glGetTexImage(GL_TEXTURE_2D, levels, GL_RGB, GL_FLOAT, &avg_color[0]);
            double luma_0 = calculateCameraLuma(glm::dot(avg_color, LUMA_MULT));

            constexpr double tau = 1.0; // Factore for how quickly eye adjusts
            if (luma_t > 0.0) // Check if luma has been intialized
                luma_t = luma_t + (luma_0 - luma_t) * (1 - glm::exp(-dt * tau));
            else
                luma_t = luma_0;
            exposure = calculateExposureFromLuma(luma_t);
        }
    }

    //
    // Upscale and blur progressively back to half resolution
    //
    // Enable additive blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);

    glUseProgram(Shaders::blur_upsample.program());
    for (int i = bloom_mip_infos.size() - 1; i > 0; i--) {
        const auto& mip = bloom_mip_infos[i];
        const auto& next_mip = bloom_mip_infos[i - 1];

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
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

static void resolveMultisampleHdrBuffer() {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, hdr_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, hdr_fbo_resolve_multisample);
    glBlitFramebuffer(0, 0, window_width, window_height, 0, 0, window_width, window_height, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
}

void drawSkybox(const Texture* skybox, const Camera &camera) {
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    // Since skybox shader writes maximum depth of 1.0, for skybox to always be we
    // need to adjust depth func
	glDepthFunc(GL_LEQUAL); 
    glDisable(GL_CULL_FACE);

    Shaders::skybox.set_macro("VOLUMETRICS", do_volumetrics);
    glUseProgram(Shaders::skybox.program());
    auto untranslated_view = glm::mat4(glm::mat3(camera.view));
    auto untranslated_view_projection = camera.projection * untranslated_view;
    glUniformMatrix4fv(Shaders::skybox.uniform("untranslated_view_projection"), 1, GL_FALSE, &untranslated_view_projection[0][0]);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->id);

    drawCube();

    // Revert state which is unexpected
	glDepthFunc(GL_LESS); 
    glEnable(GL_CULL_FACE);
}

void createRenderQueue(RenderQueue& q, const EntityManager& entity_manager, const bool lightmapping) {
    q.opaque_items.clear();
    q.transparent_items.clear();
    q.water = nullptr;

    for (int i = 0; i < ENTITY_COUNT; ++i) {
        if (entity_manager.entities[i] == nullptr) continue;
        auto m_e = reinterpret_cast<MeshEntity*>(entity_manager.entities[i]);
        if (!(entityInherits(m_e->type, MESH_ENTITY)) || m_e->mesh == nullptr) continue;

        RenderItem ri;
        ri.mesh = m_e->mesh;
        ri.state = (RenderState::Type)(RenderState::DEPTH_READ | RenderState::DEPTH_WRITE);

        if (entityInherits(m_e->type, ANIMATED_MESH_ENTITY)) {
            auto a_e = (AnimatedMeshEntity*)m_e;
            if (playing || a_e->draw_animated) {
                ri.bone_matrices = &a_e->final_bone_matrices;
            }
        }
        if (entityInherits(m_e->type, VEGETATION_ENTITY)) {
            ri.state = (RenderState::Type)(ri.state | RenderState::ALPHA_COVERAGE);
            ri.vegetation = true;
        }
        else {
            ri.state = (RenderState::Type)(ri.state | RenderState::BACK_CULL);
        }

        const auto& mesh = *m_e->mesh;
        q.opaque_items.reserve(q.opaque_items.size() + mesh.num_submeshes);
        for (uint64_t i = 0; i < mesh.num_submeshes; i++) {
            // @todo frustrum culling, occlusion culling
            // @todo sorting by vao, material, texture similarity
            auto& sri = q.opaque_items.emplace_back(ri);
            sri.submesh_i = i;
            sri.model = glm::translate(mesh.transforms[sri.submesh_i], m_e->position) * glm::mat4_cast(m_e->rotation) * glm::mat4x4(m_e->scale);

            const auto& lu = m_e->overidden_materials.find(i); // @note using the submesh index is intentional as it allows any part of a meshes material to be overriden
            if (lu == m_e->overidden_materials.end()) {
                sri.mat = &m_e->mesh->materials[mesh.material_indices[i]];
            }
            else {
                sri.mat = &lu->second;
            }

            if (sri.mat->type == MaterialType::NONE) {
                std::cerr << "Empty material on entity id: " << m_e->id.i << ", skipping\n";
                q.opaque_items.erase(q.opaque_items.end() - 1);
            }
        }
    }
    if (!lightmapping && entity_manager.water != NULLID) {
        q.water = (WaterEntity*)entity_manager.getEntity(entity_manager.water);
    }
}

static void updateRenderState(RenderState::Type& current, RenderState::Type desired) {
    auto enable  = (current ^ desired) & desired;
    auto disable = (current ^ desired) & current;

    if (enable) {
        if (enable | RenderState::FRONT_CULL) {
            glCullFace(GL_FRONT);
            glEnable(GL_CULL_FACE);
        }
        if (enable | RenderState::BACK_CULL) {
            glCullFace(GL_BACK);
            glEnable(GL_CULL_FACE);
        }
        if (enable | RenderState::ALPHA_COVERAGE) {
            glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        }
        if (enable | RenderState::DEPTH_READ) {
            glEnable(GL_DEPTH_TEST);
        }
        if (enable | RenderState::DEPTH_WRITE) {
            glDepthMask(GL_TRUE);
        }
    }
    if (disable) {
        if (disable | RenderState::FRONT_CULL || disable | RenderState::BACK_CULL) {
            glDisable(GL_CULL_FACE);
        }
        if (disable | RenderState::ALPHA_COVERAGE) {
            glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        }
        if (disable | RenderState::DEPTH_READ) {
            glDisable(GL_DEPTH_TEST);
        }
        if (disable | RenderState::DEPTH_WRITE) {
            glDepthMask(GL_FALSE);
        }
    }

    current = desired;
}

static void drawRenderItem(const RenderItem& ri, const glm::mat4x4* vp, GLuint& vao, GLuint& program, RenderState::Type& state, const bool shadow=false) {
    const auto& mesh = *ri.mesh;
    if (mesh.vao != vao) {
        glBindVertexArray(mesh.vao);
        vao = vao;
    }

    const auto& mat = *ri.mat;
    Shader& s = shadow ? Shaders::shadow : Shaders::unified;

    // Do back face culling instead of front to reduce peter-panning for shadows
    if (shadow && ri.state & RenderState::BACK_CULL) {
        updateRenderState(state, (RenderState::Type)(ri.state ^ RenderState::BACK_CULL | RenderState::FRONT_CULL));
    }
    else {
        updateRenderState(state, ri.state);
    }

    // Determine shader program from macros, @todo move to render queue construction, 
    // in future this could also be a LUT from material type
    if (!shadow) {
        s.set_macro("PBR", mat.type & MaterialType::PBR, false);
        s.set_macro("METALLIC", mat.type & MaterialType::METALLIC, false);
        s.set_macro("EMISSIVE", mat.type & MaterialType::EMISSIVE, false);
        s.set_macro("GLOBAL_ILLUMINATION", mat.type & MaterialType::LIGHTMAPPED, false);
        s.set_macro("AO", mat.type & MaterialType::AO, false);
    }

    s.set_macro("ALPHA_CLIP", mat.type & MaterialType::ALPHA_CLIP, false);
    s.set_macro("ANIMATED_BONES", ri.bone_matrices, false);
    s.set_macro("VEGETATION", ri.vegetation, false);

    // @note !!IMPORTANT this is needed to set correct shader
    s.activate_macros();

    if (s.program() != program) {
        glUseProgram(s.program());
        program = s.program();
    }

    if (ri.vegetation) {
        glUniform2f(s.uniform("wind_direction"), wind_direction.x, wind_direction.y);
        glUniform1f(s.uniform("wind_strength"), 3.0);
    }

    // Setup material uniforms and textures, skip if we are drawing a shadow,
    // the texture ids could be found in render queue step
    if (!shadow) {
        for (const auto& p : mat.textures) {
            const auto& binding = p.first;
            const auto& tex = p.second;

            glActiveTexture(GL_TEXTURE0 + (uint64_t)binding);
            glBindTexture(GL_TEXTURE_2D, tex->id);
        }

        for (const auto& p : mat.uniforms) {
            auto& name = p.first;
            auto& uniform = p.second;

            uniform.bind(s.uniform(name));
        }
    } else if (mat.type & MaterialType::ALPHA_CLIP) { // Need to bind clip texture for shadows
        const auto& lu = mat.textures.find(TextureSlot::ALPHA_CLIP);
        if (lu != mat.textures.end()) {
            const auto& binding = lu->first;
            const auto& tex = lu->second;

            glActiveTexture(GL_TEXTURE0 + (uint64_t)binding);
            glBindTexture(GL_TEXTURE_2D, tex->id);
        }
    }

    // Setup transforms
    glUniformMatrix4fv(s.uniform("model"), 1, GL_FALSE, &ri.model[0][0]);
    if (!shadow) {
        auto mvp = *vp * ri.model;
        glUniformMatrix4fv(s.uniform("mvp"), 1, GL_FALSE, &mvp[0][0]);
    }

    // If bone animated write bone matrices
    if (ri.bone_matrices) {
        writeBoneMatricesUbo(*ri.bone_matrices);
    }

    glDrawElements(mesh.draw_mode, mesh.draw_count[ri.submesh_i], mesh.draw_type, (GLvoid*)(sizeof(*mesh.indices) * mesh.draw_start[ri.submesh_i]));
}

void drawRenderQueue(const RenderQueue& q, const Texture* skybox, const Texture* irradiance_map, const Texture* prefiltered_specular_map, const Camera& camera) {
    RenderState::Type bound_state = RenderState::ALL; // Assume everything has been enabled
    updateRenderState(bound_state, RenderState::NONE); // Then disable everything
    GLuint bound_vao = GL_FALSE, bound_program = GL_FALSE;

    // Bind shared resources
    writeSharedUbo(camera);
    if (do_shadows) {
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D_ARRAY, shadow_buffer);

        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_3D, jitter_texture->id);
    }

    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_CUBE_MAP, irradiance_map->id);

    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefiltered_specular_map->id);

    glActiveTexture(GL_TEXTURE9);
    glBindTexture(GL_TEXTURE_2D, brdf_lut->id);

    if (do_volumetrics) {
        glActiveTexture(GL_TEXTURE10);
        glBindTexture(GL_TEXTURE_3D, accumulated_volumetric_volume);
    }

    Shaders::unified.set_macro("SHADOWS", do_shadows);
    Shaders::unified.set_macro("VOLUMETRICS", do_volumetrics);

    for (const auto& ri : q.opaque_items) {
        drawRenderItem(ri, &camera.vp, bound_vao, bound_program, bound_state);
    }

    drawSkybox(skybox, camera);

    for (const auto& ri : q.transparent_items) {
        drawRenderItem(ri, &camera.vp, bound_vao, bound_program, bound_state);
    }
    
    // @todo integrate with transparent render queue
    if (q.water) {
        Shaders::water.set_macro("SHADOWS", do_shadows);
        Shaders::water.set_macro("VOLUMETRICS", do_volumetrics);
        updateRenderState(bound_state, RenderState::NONE);

        glUseProgram(Shaders::water.program());
        if (do_msaa) {
            resolveMultisampleHdrBuffer();
            bindHdr();
        }

        glActiveTexture(GL_TEXTURE0);
        if (do_msaa)
            glBindTexture(GL_TEXTURE_2D, hdr_buffer_resolve_multisample);
        else
            glBindTexture(GL_TEXTURE_2D, hdr_buffer);
        // Copy depth buffer
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, hdr_depth_copy);
        if(!do_msaa) // resolving copies depth already
            glCopyTextureSubImage2D(hdr_depth_copy, 0, 0, 0, 0, 0, window_width, window_height);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, simplex_gradient->id);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, simplex_value->id);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, water_collider_buffers[water_collider_final_fbo]);
    
        auto model = createModelMatrix(q.water->position, glm::quat(), q.water->scale);
        auto mvp = camera.vp * model;
        glUniformMatrix4fv(Shaders::water.uniform("mvp"), 1, GL_FALSE, &mvp[0][0]);
        glUniformMatrix4fv(Shaders::water.uniform("model"), 1, GL_FALSE, &model[0][0]);
        glUniform4fv(Shaders::water.uniform("shallow_color"), 1, &q.water->shallow_color[0]);
        glUniform4fv(Shaders::water.uniform("deep_color"), 1, &q.water->deep_color[0]);
        glUniform4fv(Shaders::water.uniform("foam_color"), 1, &q.water->foam_color[0]);

        if (water_grid.complete) {
            glBindVertexArray(water_grid.vao);
            glDrawElements(water_grid.draw_mode, water_grid.draw_count[0], water_grid.draw_type, 
                            (GLvoid*)(sizeof(*water_grid.indices)*water_grid.draw_start[0]));
        }
    }
    
    if (do_msaa) {
        resolveMultisampleHdrBuffer();
    }
}

// Since shadow buffer wont need to be bound otherwise just combine these operations
void drawRenderQueueShadows(const RenderQueue& q) {
    // @todo make these global and persistant/renderer class
    RenderState::Type bound_state = RenderState::ALL; // Assume everything has been enabled
    updateRenderState(bound_state, RenderState::NONE); // Then disable everything
    GLuint bound_vao = GL_FALSE, bound_program = GL_FALSE;

    glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);
    glViewport(0, 0, shadow_size, shadow_size);
    glClear(GL_DEPTH_BUFFER_BIT);

    writeShadowVpsUbo();

    // Only calculate shadows for opaque objects, @todo partial shadows with transparency?
    for (const auto& ri : q.opaque_items) {
        drawRenderItem(ri, nullptr, bound_vao, bound_program, bound_state, true);
    }

    // Reset bound framebuffer and return to original viewport
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, window_width, window_height);
}

void bindBackbuffer(){
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void drawPost(Texture *skybox, const Camera &camera){
    // Draw screen space quad so clearing is unnecessary
    //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);

    Shaders::post.set_macro("BLOOM", do_bloom);
    glUseProgram(Shaders::post.program());

    glUniform2f(Shaders::post.uniform("resolution"), window_width, window_height);
    glUniform1f(Shaders::post.uniform("exposure"), exposure);

    glActiveTexture(GL_TEXTURE0);
    if (do_msaa) glBindTexture(GL_TEXTURE_2D, hdr_buffer_resolve_multisample);
    else         glBindTexture(GL_TEXTURE_2D, hdr_buffer);

    if (do_bloom) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, bloom_mip_infos[0].texture);
    }

    glActiveTexture(GL_TEXTURE2);
    if (do_msaa) glBindTexture(GL_TEXTURE_2D, hdr_depth_copy);
    else         glBindTexture(GL_TEXTURE_2D, hdr_depth);

    // @todo expose shader macro  values
    // Rudimentary fog
    /*auto untranslated_view = glm::mat4(glm::mat3(camera.view));
    auto inverse_projection_untranslated_view = glm::inverse(camera.projection * untranslated_view);
    glUniformMatrix4fv(post.uniform("inverse_projection_untranslated_view"), 1, GL_FALSE, &inverse_projection_untranslated_view[0][0]);*/
    // SSAO
    //glUniformMatrix4fv(post.uniform("projection"), 1, GL_FALSE, &camera.projection[0][0]);
    /*glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_3D, jitter_texture->id);*/
    //glUniform3fv(post.uniform("camera_position"), 1, &camera.position[0]);

    /*glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->id);*/

    drawQuad();
}

// convert HDR cubemap environment map to scaled down irradiance cubemap
// @note that out_tex should be an empty texture
void convoluteIrradianceFromCubemap(Texture *in_tex, Texture *out_tex, GLint format) {
    out_tex->resolution = glm::ivec2(32, 32); // @hardcoded

    GLuint FBO, RBO;
    glGenFramebuffers(1, &FBO);
    glGenRenderbuffers(1, &RBO);

    glBindFramebuffer(GL_FRAMEBUFFER, FBO);
    glBindRenderbuffer(GL_RENDERBUFFER, RBO);

    static const glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    static const glm::mat4 views[] =
    {
       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };

    // Create output cubemap
    glGenTextures(1, &out_tex->id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, out_tex->id);
    for (unsigned int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, out_tex->resolution.x, out_tex->resolution.y, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glUseProgram(Shaders::diffuse_convolution.program());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, in_tex->id);

    glViewport(0, 0, out_tex->resolution.x, out_tex->resolution.y);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);
    for (unsigned int i = 0; i < 6; ++i) {
        auto vp = projection * glm::mat4(glm::mat3(views[i])); // removes translation @speed
        glUniformMatrix4fv(Shaders::diffuse_convolution.uniform("vp"), 1, GL_FALSE, &vp[0][0]);
        
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, out_tex->id, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        drawCube();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &FBO);
    glDeleteRenderbuffers(1, &RBO);
}

constexpr int MAX_SPECULAR_MIP = 5;
// convert HDR cubemap environment map to specular pre computed map for different roughnesses
// @note that out_tex should be an empty texture
void convoluteSpecularFromCubemap(Texture* in_tex, Texture* out_tex, GLint format) {
    out_tex->resolution = glm::ivec2(128, 128); // @hardcoded may be too little for large metallic surfaces

    GLuint FBO, RBO;
    glGenFramebuffers(1, &FBO);
    glGenRenderbuffers(1, &RBO);

    glBindFramebuffer(GL_FRAMEBUFFER, FBO);
    glBindRenderbuffer(GL_RENDERBUFFER, RBO);

    static const glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    static const glm::mat4 views[] =
    {
       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };

    // Create output cubemap
    glGenTextures(1, &out_tex->id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, out_tex->id);
    for (unsigned int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, out_tex->resolution.x, out_tex->resolution.y, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenerateMipmap(GL_TEXTURE_CUBE_MAP); // Since we store different roughnesses in mip maps

    glUseProgram(Shaders::specular_convolution.program());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, in_tex->id);
    float texelSphericalArea = 4.0 * PI / (6.0 * in_tex->resolution[0] * in_tex->resolution[0]);
    glUniform1f(Shaders::specular_convolution.uniform("texelSphericalArea"), texelSphericalArea);

    glm::ivec2 mip_resolution = out_tex->resolution;
    for (unsigned int mip = 0; mip < MAX_SPECULAR_MIP; ++mip) {
        glViewport(0, 0, mip_resolution.x, mip_resolution.y);
        
        float roughness = (float)mip / (float)(MAX_SPECULAR_MIP - 1);
        glUniform1f(Shaders::specular_convolution.uniform("roughness"), roughness);

        for (unsigned int i = 0; i < 6; ++i) {
            auto vp = projection * glm::mat4(glm::mat3(views[i])); // removes translation @speed precompute
            glUniformMatrix4fv(Shaders::specular_convolution.uniform("vp"), 1, GL_FALSE, &vp[0][0]);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, out_tex->id, mip);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            drawCube();
        }

        mip_resolution /= 2; // Set resolution appropriate to mip
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &FBO);
    glDeleteRenderbuffers(1, &RBO);
}

bool createEnvironmentFromCubemap(Environment& env, AssetManager &asset_manager, const std::array<std::string, FACE_NUM_FACES>& paths, GLint format) {
    env.skybox = asset_manager.createTexture("skybox");
    if (!asset_manager.loadCubemapTexture(env.skybox, paths, format, GL_REPEAT, true)) {
        std::cerr << "Error loading cubemap\n";
        return false;
    }

    env.skybox_irradiance = asset_manager.createTexture("skybox_irradiance");
    convoluteIrradianceFromCubemap(env.skybox, env.skybox_irradiance, format);
    env.skybox_specular = asset_manager.createTexture("skybox_specular");
    convoluteSpecularFromCubemap(env.skybox, env.skybox_specular, format);
    initBRDFLut(asset_manager);

    return true;
}

// Copied from Nvidia's GPU gems 2 for use with PCF
// https://developer.nvidia.com/gpugems/gpugems2/part-ii-shading-lighting-and-shadows/chapter-17-efficient-soft-edged-shadows-using
// Create 3D texture for per-pixel jittered offset lookup
Texture *createJitter3DTexture(AssetManager &asset_manager, int size, int samples_u, int samples_v) {
    auto tex = asset_manager.createTexture("jitter");

    glGenTextures(1, &tex->id);
    glBindTexture(GL_TEXTURE_3D, tex->id);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);

    signed char* data = new signed char[size * size * samples_u * samples_v * 4 / 2];

    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            float rot_offset = ((float)rand() / RAND_MAX - 1) * 2 * 3.1415926f;
            for (int k = 0; k < samples_u * samples_v / 2; k++) {

                int x, y;
                glm::vec4 v;

                x = k % (samples_u / 2);
                y = (samples_v - 1) - k / (samples_u / 2);

                // generate points on a regular samples_u x samples_v rectangular grid
                v[0] = (float)(x * 2 + 0.5f) / samples_u;
                v[1] = (float)(y + 0.5f) / samples_v;
                v[2] = (float)(x * 2 + 1 + 0.5f) / samples_u;
                v[3] = v[1];

                // jitter position
                v[0] += ((float)rand() * 2 / RAND_MAX - 1) * (0.5f / samples_u);
                v[1] += ((float)rand() * 2 / RAND_MAX - 1) * (0.5f / samples_v);
                v[2] += ((float)rand() * 2 / RAND_MAX - 1) * (0.5f / samples_u);
                v[3] += ((float)rand() * 2 / RAND_MAX - 1) * (0.5f / samples_v);

                // warp to disk
                glm::vec4 d;
                d[0] = sqrtf(v[1]) * cosf(2 * 3.1415926f * v[0]);
                d[1] = sqrtf(v[1]) * sinf(2 * 3.1415926f * v[0]);
                d[2] = sqrtf(v[3]) * cosf(2 * 3.1415926f * v[2]);
                d[3] = sqrtf(v[3]) * sinf(2 * 3.1415926f * v[2]);

                data[(k * size * size + j * size + i) * 4 + 0] = (signed char)(d[0] * 127);
                data[(k * size * size + j * size + i) * 4 + 1] = (signed char)(d[1] * 127);
                data[(k * size * size + j * size + i) * 4 + 2] = (signed char)(d[2] * 127);
                data[(k * size * size + j * size + i) * 4 + 3] = (signed char)(d[3] * 127);
            }
        }
    }

    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, size, size, samples_u * samples_v / 2, 0, GL_RGBA, GL_BYTE, data);

    delete[] data;

    return tex;
}