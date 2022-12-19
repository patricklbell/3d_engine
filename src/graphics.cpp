#include <algorithm>
#include <limits>
#include <stdio.h>
#include <stdlib.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/detail/type_mat.hpp>
#include <glm/gtc/matrix_access.hpp>

#include <camera/core.hpp>
#include <shader/globals.hpp>

#include <utilities/math.hpp>

#include "renderer.hpp"
#include "assets.hpp"
#include "globals.hpp"
#include "entities.hpp"
#include "editor.hpp"
#include "game_behaviour.hpp"

#include "graphics.hpp"

int    window_width;
int    window_height;
bool   window_resized;

namespace graphics {
    // 
    // Bloom
    //
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
    // Reloads ubo everytime, @todo integrate this with gl_state to avoid rebinding/copying
    glBindBuffer(GL_UNIFORM_BUFFER, bone_matrices_ubo);
    for (size_t i = 0; i < MAX_BONES; ++i)
    {
        glBufferSubData(GL_UNIFORM_BUFFER, i * sizeof(glm::mat4x4), sizeof(glm::mat4x4), &final_bone_matrices[i]);
    }
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

static glm::mat4x4 getShadowMatrixFromFrustrum(const Camera& camera, double near_plane, double far_plane, glm::vec3 light_direction) {
    // Make shadow's view target the center of the camera frustrum by averaging frustrum corners
    // maybe you can just calculate from view direction and near and far
    const auto camera_projection_alt = glm::perspective((double)camera.frustrum.fov_y, (double)camera.frustrum.aspect_ratio, near_plane, far_plane);
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

    const auto shadow_view = glm::lookAt(-glm::dvec3(light_direction) + center, center, glm::dvec3(camera.up));
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
    //const auto shadow_projection = glm::ortho(min_x, max_x, min_y, max_y, -max_z, -min_z);
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

void updateShadowVP(const Camera& camera, glm::vec3 light_direction) {
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

        shadow_vps[i] = getShadowMatrixFromFrustrum(camera, snp, sfp, light_direction);

        shadow_cascade_distances[i] = sfp;
        snp = sfp;
    }

    writeShadowVpsUbo();
}

void initShadowFbo() {
    glGenFramebuffers(1, &shadow_fbo);
    gl_state.bind_framebuffer(shadow_fbo);

    glGenTextures(1, &shadow_buffer);
    gl_state.bind_texture(0, shadow_buffer, GL_TEXTURE_2D_ARRAY);
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
    gl_state.bind_texture(0, brdf_lut->id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, brdf_lut->resolution.x, brdf_lut->resolution.y, 0, GL_RG, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    gl_state.bind_framebuffer(FBO);
    gl_state.bind_renderbuffer(RBO);

    gl_state.bind_program(Shaders::generate_brdf_lut.program());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdf_lut->id, 0);
    gl_state.bind_viewport(brdf_lut->resolution.x, brdf_lut->resolution.y);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawQuad();

    gl_state.bind_renderbuffer(GL_FALSE);
}

void initWaterColliderFbo() {
    glGenFramebuffers(2, water_collider_fbos);
    glGenTextures(2, water_collider_buffers);

    static const GLuint attachments[] = { GL_COLOR_ATTACHMENT0 };
    for (unsigned int i = 0; i < 2; i++) {
        gl_state.bind_framebuffer(water_collider_fbos[i]);
        glDrawBuffers(1, attachments);
        gl_state.bind_texture(0, water_collider_buffers[i]);
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
}

void bindDrawWaterColliderMap(const RenderQueue& q, WaterEntity* water) {
    gl_state.bind_viewport(WATER_COLLIDER_SIZE, WATER_COLLIDER_SIZE);
    gl_state.set_flags(GlFlags::DEPTH_WRITE);

    gl_state.bind_framebuffer(water_collider_fbos[0]);
    glClear(GL_COLOR_BUFFER_BIT);

    gl_state.bind_program(Shaders::plane_projection.program());

    auto water_grid_scale = water->scale;
    // Remove y component of scale, shouldn't technically be necessary but helps with debugging
    water_grid_scale[1][1] = 1.0f;
    auto inv_water_grid = glm::inverse(createModelMatrix(water->position, glm::quat(), water_grid_scale));
    for (const auto& ri : q.opaque_items) {
        auto model_inv_water_grid = inv_water_grid * ri.model;
        glUniformMatrix4fv(Shaders::plane_projection.uniform("model"), 1, GL_FALSE, &model_inv_water_grid[0][0]);

        auto& mesh = *ri.mesh;
        gl_state.bind_vao(mesh.vao);
        glDrawElements(mesh.draw_mode, mesh.draw_count[ri.submesh_i], mesh.draw_type, (GLvoid*)(sizeof(*mesh.indices) * mesh.draw_start[ri.submesh_i]));
    }
}

void distanceTransformWaterFbo(WaterEntity* water) {
    constexpr uint64_t num_steps = nextPowerOf2(WATER_COLLIDER_SIZE, WATER_COLLIDER_SIZE) - 2;

    gl_state.set_flags(GlFlags::NONE);
    gl_state.bind_program(Shaders::jump_flood.program());

    glUniform1f(Shaders::jump_flood.uniform("num_steps"), (float)num_steps);
    glUniform2f(Shaders::jump_flood.uniform("resolution"), WATER_COLLIDER_SIZE, WATER_COLLIDER_SIZE);

    gl_state.bind_viewport(WATER_COLLIDER_SIZE, WATER_COLLIDER_SIZE);
    for (int step = 0; step <= num_steps; step++) {
        // Last iteration convert jfa to distance transform
        if (step != num_steps) {
            glUniform1f(Shaders::jump_flood.uniform("step"), (float)step);
        }
        else {
            gl_state.bind_program(Shaders::jfa_to_distance.program());
            glUniform2f(Shaders::jfa_to_distance.uniform("dimensions"), water->scale[0][0], water->scale[2][2]);
        }

        gl_state.bind_framebuffer(water_collider_fbos[(step + 1) % 2]);
        gl_state.bind_texture(0, water_collider_buffers[step % 2]);

        drawQuad();
    }

    gl_state.bind_texture_any(water_collider_buffers[(num_steps + 1) % 2]);
    glGenerateMipmap(GL_TEXTURE_2D);
    water_collider_final_fbo = (num_steps + 1) % 2;

    gl_state.bind_program(Shaders::gaussian_blur.program());
    for (unsigned int i = 0; i < 2; i++) {
        gl_state.bind_framebuffer(water_collider_fbos[(num_steps + i) % 2]);

        glUniform1i(Shaders::gaussian_blur.uniform("horizontal"), (int)i);
        gl_state.bind_texture(0, water_collider_buffers[(num_steps + i + 1) % 2]);

        drawQuad();
    }
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
    gl_state.bind_framebuffer(bloom_fbo);

    auto mip_size = glm::vec2(window_width, window_height);
    for (int i = 0; i < BLOOM_DOWNSAMPLES; ++i) {
        auto& mip = bloom_mip_infos.emplace_back();

        mip_size *= 0.5f;
        mip.size = mip_size;

        glGenTextures(1, &mip.texture);
        gl_state.bind_texture(0, mip.texture);
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

    gl_state.bind_framebuffer(hdr_fbo);

    if (do_msaa) {
        gl_state.bind_texture_any(hdr_buffer, GL_TEXTURE_2D_MULTISAMPLE);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, MSAA_SAMPLES, GL_RGBA16F, window_width, window_height, GL_TRUE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, hdr_buffer, 0);
    }
    else {
        gl_state.bind_texture_any(hdr_buffer);
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
        gl_state.bind_texture_any(hdr_depth, GL_TEXTURE_2D_MULTISAMPLE);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, MSAA_SAMPLES, GL_DEPTH24_STENCIL8, window_width, window_height, GL_TRUE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, hdr_depth, 0);
    }
    else {
        gl_state.bind_texture_any(hdr_depth);
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
    gl_state.bind_texture_any(hdr_depth_copy);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, window_width, window_height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Since we perform post processing on screen texture we need an intermediate fbo to resolve to
    if (do_msaa) {
        gl_state.bind_framebuffer(hdr_fbo_resolve_multisample);

        gl_state.bind_texture_any(hdr_depth_copy); // @note Might be redundant
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, hdr_depth_copy, 0);

        gl_state.bind_texture_any(hdr_buffer_resolve_multisample);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, window_width, window_height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hdr_buffer_resolve_multisample, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cerr << "Hdr resolve framebuffer not complete, error code: " << glCheckFramebufferStatus(GL_FRAMEBUFFER) << ".\n";
    }

    gl_state.bind_framebuffer(GL_FALSE);
}

static void initSharedUbo() {
    glGenBuffers(1, &shared_uniforms_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, shared_uniforms_ubo);
    glBufferData(GL_UNIFORM_BUFFER, 192, NULL, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, shared_uniforms_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

static void writeSharedUbo(const Camera& camera, const Environment& env) {
    // @note if something else binds another ubo to 0 then this will be overwritten
    glBindBuffer(GL_UNIFORM_BUFFER, shared_uniforms_ubo);
    int offset = 0;
    glBufferSubData(GL_UNIFORM_BUFFER, offset, 64, &camera.view[0][0]); offset += 4 * 16;
    glm::mat4 vp = camera.projection * camera.view;
    glBufferSubData(GL_UNIFORM_BUFFER, offset, 64, &vp[0][0]); offset += 4 * 16;
    glBufferSubData(GL_UNIFORM_BUFFER, offset, 16, &env.sun_direction[0]); offset += 16;
    glBufferSubData(GL_UNIFORM_BUFFER, offset, 16, &env.sun_color[0]); offset += 16;
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
    gl_state.bind_texture_any(accumulated_volumetric_volume, GL_TEXTURE_3D);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // We don't want to volumetrics which we haven't calculated
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA16F, VOLUMETRIC_RESOLUTION.x, VOLUMETRIC_RESOLUTION.y, VOLUMETRIC_RESOLUTION.z);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, VOLUMETRIC_RESOLUTION.x, VOLUMETRIC_RESOLUTION.y, VOLUMETRIC_RESOLUTION.z, GL_RGBA16F, GL_HALF_FLOAT, NULL);

    glGenTextures(NUM_TEMPORAL_VOLUMES, temporal_integration_volume);
    for (int i = 0; i < NUM_TEMPORAL_VOLUMES; ++i) {
        gl_state.bind_texture_any(temporal_integration_volume[i], GL_TEXTURE_3D);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // We don't want to volumetrics which we haven't calculated
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA16F, VOLUMETRIC_RESOLUTION.x, VOLUMETRIC_RESOLUTION.y, VOLUMETRIC_RESOLUTION.z);
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, VOLUMETRIC_RESOLUTION.x, VOLUMETRIC_RESOLUTION.y, VOLUMETRIC_RESOLUTION.z, GL_RGBA16F, GL_HALF_FLOAT, NULL);
    }
}

void computeVolumetrics(const Environment& env, uint64_t frame_i, const Camera& camera) {
    // @todo integrate with draw entities so we don't send ubo,
    // in future it would be better to have a system for write ubos easily and checking if something has changed
    writeSharedUbo(camera, env);

    // Convert fog volumes, for now just a global fog, into camera fitted color&density 3D texture
    auto& vi = Shaders::volumetric_integration;
    gl_state.bind_program(vi.program());

    const auto& properties = env.fog;
    glUniform1f(vi.uniform("anisotropy"), properties.anisotropy);
    glUniform1f(vi.uniform("density"), properties.density);
    glUniform1f(vi.uniform("noise_scale"), properties.noise_scale);
    glUniform1f(vi.uniform("noise_amount"), properties.noise_amount);
    glUniform1i(vi.uniform("do_accumulation"), frame_i != 0);
    glUniform3iv(vi.uniform("vol_size"), 1, &VOLUMETRIC_RESOLUTION[0]);
    glUniform3fv(vi.uniform("wind_direction"), 1, &wind_direction[0]);
    glUniform1f(vi.uniform("wind_strength"), wind_strength);
    
    const auto inv_vp = glm::inverse(camera.vp);
    static auto prev_vp = camera.vp;
    // https://www.khronos.org/opengl/wiki/Compute_eye_space_from_window_space
    glUniform3f(vi.uniform("t1t2e1"), camera.projection[2][2], camera.projection[2][3], camera.projection[3][2]);
    glUniformMatrix4fv(vi.uniform("inv_vp"), 1, GL_FALSE, &inv_vp[0][0]);
    glUniformMatrix4fv(vi.uniform("prev_vp"), 1, GL_FALSE, &prev_vp[0][0]);
    prev_vp = camera.vp;

    gl_state.bind_texture(0, blue_noise_textures[frame_i % NUM_BLUE_NOISE_TEXTURES]->id);
    gl_state.bind_texture(1, temporal_integration_volume[(frame_i + 1) % NUM_TEMPORAL_VOLUMES], GL_TEXTURE_3D);
    if (do_shadows) {
        gl_state.bind_texture(5, shadow_buffer, GL_TEXTURE_2D_ARRAY);
        gl_state.bind_texture(6, jitter_texture->id, GL_TEXTURE_3D);
    }

    glBindImageTexture(0, temporal_integration_volume[frame_i % NUM_TEMPORAL_VOLUMES], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    glm::uvec3 compute_size = glm::ceil(glm::fvec3(VOLUMETRIC_RESOLUTION) / glm::fvec3(VOLUMETRIC_LOCAL_SIZE));
    glDispatchCompute(compute_size.x, compute_size.y, compute_size.z);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT); // @todo do other rendering steps while waiting

    // Raymarch through fog volume to accumulate scattering and transmission
    auto& vr = Shaders::volumetric_raymarch;
    gl_state.bind_program(vr.program());

    glUniform3iv(vr.uniform("vol_size"), 1, &VOLUMETRIC_RESOLUTION[0]);

    gl_state.bind_texture(0, temporal_integration_volume[frame_i % NUM_TEMPORAL_VOLUMES], GL_TEXTURE_3D);

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

    gl_state.bind_vao(quad.vao);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo);

    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), &quad_vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

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

    gl_state.bind_vao(cube.vao);
    glBindBuffer(GL_ARRAY_BUFFER, cube_vbo);

    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), &cube_vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

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

    gl_state.bind_vao(line_cube.vao);
    glBindBuffer(GL_ARRAY_BUFFER, line_cube_vbo);

    glBufferData(GL_ARRAY_BUFFER, sizeof(line_cube_vertices), &line_cube_vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

    line_cube.draw_count = (GLint*)malloc(sizeof(GLint));
    line_cube.draw_start = (GLint*)malloc(sizeof(GLint));
    line_cube.draw_mode = GL_LINES;
    line_cube.draw_type = GL_UNSIGNED_SHORT;

    line_cube.draw_start[0] = 0;
    line_cube.draw_count[0] = sizeof(line_cube_vertices) / (2.0 * sizeof(*line_cube_vertices));

    asset_manager.loadMeshFile(&water_grid, "data/mesh/water_grid.mesh");
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

    initVolumetrics();
    initShadowFbo();
    initSharedUbo();
    initBloomFbo();
    initBoneMatricesUbo();
    initWaterColliderFbo();
}

void drawCube() {
    gl_state.bind_vao(cube.vao);
    glDrawArrays(cube.draw_mode, cube.draw_start[0], cube.draw_count[0]);
}
void drawLineCube() {
    gl_state.bind_vao(line_cube.vao);
    glDrawArrays(line_cube.draw_mode, line_cube.draw_start[0], line_cube.draw_count[0]);
}
void drawQuad() {
    gl_state.bind_vao(quad.vao);
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
    gl_state.bind_framebuffer(bloom_fbo);
    gl_state.set_flags(GlFlags::NONE);

    // 
    // Progressively downsample screen texture
    //
    gl_state.bind_program(Shaders::downsample.program());
    glUniform2f(Shaders::downsample.uniform("resolution"), window_width, window_height);

    if(do_msaa) 
        gl_state.bind_texture(0, hdr_buffer_resolve_multisample);
    else        
        gl_state.bind_texture(0, hdr_buffer);

    for (int i = 0; i < bloom_mip_infos.size(); ++i) {
        const auto& mip = bloom_mip_infos[i];

        gl_state.bind_viewport(mip.size.x, mip.size.y);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mip.texture, 0);

        int is_mip0 = i == bloom_mip_infos.size() - 1;
        glUniform1i(Shaders::downsample.uniform("is_mip0"), is_mip0);

        drawQuad();

        // Set next iterations properties
        glUniform2fv(Shaders::downsample.uniform("resolution"), 1, &mip.size[0]);
        gl_state.bind_texture(0, mip.texture);

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
    gl_state.add_flags(GlFlags::BLEND);
    gl_state.set_blend_mode(GlBlendMode::ADDITIVE);

    gl_state.bind_program(Shaders::blur_upsample.program());
    for (int i = bloom_mip_infos.size() - 1; i > 0; i--) {
        const auto& mip = bloom_mip_infos[i];
        const auto& next_mip = bloom_mip_infos[i - 1];

        gl_state.bind_texture(0, mip.texture);

        gl_state.bind_viewport(next_mip.size.x, next_mip.size.y);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, next_mip.texture, 0);

        drawQuad();
    }

    gl_state.set_blend_mode(GlBlendMode::OVERWRITE);
}

void bindHdr() {
    gl_state.bind_framebuffer(hdr_fbo);
    gl_state.bind_viewport(window_width, window_height);
}

void bindBackbuffer() {
    gl_state.bind_framebuffer(GL_FALSE);
    gl_state.bind_viewport(window_width, window_height);
}

void clearFramebuffer() {
    gl_state.add_flags(GlFlags::DEPTH_WRITE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

static void resolveMultisampleHdrBuffer() {
    gl_state.bind_framebuffer(hdr_fbo, GlBufferFlags::READ);
    gl_state.bind_framebuffer(hdr_fbo_resolve_multisample, GlBufferFlags::WRITE);
    glBlitFramebuffer(0, 0, window_width, window_height, 0, 0, window_width, window_height, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
}

void drawSkybox(const Texture* skybox, const Camera &camera) {
    gl_state.set_flags(GlFlags::DEPTH_READ | GlFlags::DEPTH_WRITE);
    glDepthFunc(GL_LEQUAL);

    Shaders::skybox.set_macro("VOLUMETRICS", do_volumetrics);
    gl_state.bind_program(Shaders::skybox.program());

    auto untranslated_view = glm::mat4(glm::mat3(camera.view));
    auto untranslated_view_projection = camera.projection * untranslated_view;
    glUniformMatrix4fv(Shaders::skybox.uniform("untranslated_view_projection"), 1, GL_FALSE, &untranslated_view_projection[0][0]);
    gl_state.bind_texture(TextureSlot::SKYBOX, skybox->id, GL_TEXTURE_CUBE_MAP);

    drawCube();

    glDepthFunc(GL_LESS);
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
        ri.flags = GlFlags::DEPTH_READ | GlFlags::DEPTH_WRITE | GlFlags::CULL;

        if (entityInherits(m_e->type, ANIMATED_MESH_ENTITY)) {
            if (lightmapping) 
                continue;

            auto a_e = (AnimatedMeshEntity*)m_e;
            if (gamestate.is_active || a_e->draw_animated) {
                ri.bone_matrices = &a_e->final_bone_matrices;
            }
        }

        const auto& mesh = *m_e->mesh;
        q.opaque_items.reserve(q.opaque_items.size() + mesh.num_submeshes);
        for (uint64_t i = 0; i < mesh.num_submeshes; i++) {
            // @todo frustrum culling, occlusion culling
            // @todo sorting by vao, material, texture similarity
            auto& sri = q.opaque_items.emplace_back(ri);
            sri.submesh_i = i;
            sri.model = createModelMatrix(mesh.transforms[i], m_e->position, m_e->rotation, m_e->scale);
            sri.draw_shadow = m_e->casts_shadow;
            sri.aabb = &mesh.aabbs[i];

            const auto& lu = m_e->overidden_materials.find(i); // @note using the submesh index is intentional as it allows any part of a meshes material to be overriden
            if (lu == m_e->overidden_materials.end()) {
                sri.mat = &m_e->mesh->materials[mesh.material_indices[i]];
            } else {
                sri.mat = &lu->second;
            }

            if (!!(sri.mat->type & MaterialType::VEGETATION)) {
                ri.flags = ri.flags | GlFlags::ALPHA_COVERAGE;
            }
            if (!!(sri.mat->type & MaterialType::ALPHA_CLIP)) {
                ri.flags = ri.flags & (~GlFlags::CULL);
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

void frustrumCullRenderQueue(RenderQueue& q, const Camera& camera) {
    auto f_planes = FrustrumCollider(camera);

    for (int i = 0; i < 2; i++) {
        auto& items = (i == 0) ? q.opaque_items : q.transparent_items;

        for (auto& ri : items) {
            auto t_aabb = transformAABB(*ri.aabb, ri.model);

            ri.culled = !f_planes.isAabbInFrustrum(t_aabb);
        }
    }
}

void uncullRenderQueue(RenderQueue& q) {
    for (int i = 0; i < 2; i++) {
        auto& items = (i == 0) ? q.opaque_items : q.transparent_items;

        for (auto& ri : items) {
            ri.culled = false;
        }
    }
}

static void drawRenderItem(const RenderItem& ri, const glm::mat4x4* vp, const bool shadow=false) {
    if ((shadow && !ri.draw_shadow) || (!shadow && ri.culled))
        return;

    if (shadow) { // Get rid of culling if we are drawing a shadow, its sometimes suggested to use front face culling but this doesn't seem to work well
        gl_state.set_flags(ri.flags & (~GlFlags::CULL));
    }
    else {
        gl_state.set_flags(ri.flags);
    }
    
    Shader& s = shadow ? Shaders::shadow : Shaders::unified;
    // Determine shader program from macros, @todo move to render queue construction, 
    // in future this could also be a LUT from material type
    const auto& mat = *ri.mat;
    s.set_macro("ANIMATED_BONES", ri.bone_matrices, false);
    s.set_macro("ALPHA_CLIP", !!(mat.type & MaterialType::ALPHA_CLIP), false);
    s.set_macro("VEGETATION", !!(mat.type & MaterialType::VEGETATION), false);
    if (!shadow) {
        s.set_macro("PBR", !!(mat.type & MaterialType::PBR), false);
        s.set_macro("METALLIC", !!(mat.type & MaterialType::METALLIC), false);
        s.set_macro("EMISSIVE", !!(mat.type & MaterialType::EMISSIVE), false);
        s.set_macro("GLOBAL_ILLUMINATION", !!(mat.type & MaterialType::LIGHTMAPPED), false);
        s.set_macro("AO", !!(mat.type & MaterialType::AO), false);
    }
    s.activate_macros(); // @note !!IMPORTANT!! this is needed to set correct shader
    gl_state.bind_program(s.program());

    // Setup material uniforms and textures, skip if we are drawing a shadow,
    // the texture ids could be found in render queue step
    if (!shadow) {
        for (const auto& p : mat.textures) {
            const auto& binding = p.first;
            const auto& tex = p.second;

            gl_state.bind_texture(binding, tex->id);
        }

        for (const auto& p : mat.uniforms) {
            auto& name = p.first;
            auto& uniform = p.second;

            uniform.bind(s.uniform(name));
        }
    } else if (!!(mat.type & MaterialType::ALPHA_CLIP)) { // Only need to bind clip texture for shadows
        const auto& lu = mat.textures.find(TextureSlot::ALPHA_CLIP);
        if (lu != mat.textures.end()) {
            const auto& binding = lu->first;
            const auto& tex = lu->second;

            gl_state.bind_texture(binding, tex->id);
        }
    }

    // Setup transforms and other uniforms
    glUniformMatrix4fv(s.uniform("model"), 1, GL_FALSE, &ri.model[0][0]);
    if (!shadow && vp) {
        auto mvp = (*vp) * ri.model;
        glUniformMatrix4fv(s.uniform("mvp"), 1, GL_FALSE, &mvp[0][0]);
    }
    if (!!(mat.type & MaterialType::VEGETATION)) {
        glUniform2f(s.uniform("wind_direction"), wind_direction.x, wind_direction.y);
        glUniform1f(s.uniform("wind_strength"), 3.0);
        if (shadow)
            glUniform1f(s.uniform("time"), glfwGetTime());
    }

    // If bone animated write bone matrices
    if (ri.bone_matrices) {
        writeBoneMatricesUbo(*ri.bone_matrices);
    }

    const auto& mesh = *ri.mesh;
    gl_state.bind_vao(mesh.vao);
    glDrawElements(mesh.draw_mode, mesh.draw_count[ri.submesh_i], mesh.draw_type, (GLvoid*)(sizeof(*mesh.indices) * mesh.draw_start[ri.submesh_i]));
}

void drawRenderQueue(const RenderQueue& q, Environment& env, const Camera& camera) {
    // Bind shared resources
    writeSharedUbo(camera, env);
    if (do_shadows) {
        gl_state.bind_texture(TextureSlot::SHADOW_BUFFER, shadow_buffer, GL_TEXTURE_2D_ARRAY);
        gl_state.bind_texture(TextureSlot::SHADOW_JITTER, jitter_texture->id, GL_TEXTURE_3D);
    }
    if (do_volumetrics) {
        gl_state.bind_texture(TextureSlot::VOLUMETRICS, accumulated_volumetric_volume, GL_TEXTURE_3D);
    }

    gl_state.bind_texture(TextureSlot::ENV_IRRADIANCE, env.skybox_irradiance->id, GL_TEXTURE_CUBE_MAP);
    gl_state.bind_texture(TextureSlot::ENV_SPECULAR, env.skybox_specular->id, GL_TEXTURE_CUBE_MAP);
    gl_state.bind_texture(TextureSlot::BRDF_LUT, brdf_lut->id);

    Shaders::unified.set_macro("SHADOWS", do_shadows);
    Shaders::unified.set_macro("VOLUMETRICS", do_volumetrics);

    for (const auto& ri : q.opaque_items) {
        drawRenderItem(ri, &camera.vp);
    }

    drawSkybox(env.skybox, camera);

    for (const auto& ri : q.transparent_items) {
        drawRenderItem(ri, &camera.vp);
    }
    
    // @todo integrate with transparent render queue
    if (q.water) {
        gl_state.set_flags(GlFlags::CULL | GlFlags::DEPTH_READ | GlFlags::BLEND);
        gl_state.set_blend_mode(GlBlendMode::ALPHA);

        Shaders::water.set_macro("SHADOWS", do_shadows, false);
        Shaders::water.set_macro("VOLUMETRICS", do_volumetrics, false);
        Shaders::water.activate_macros();
        gl_state.bind_program(Shaders::water.program());
        
        // Copy depth buffer
        glCopyTextureSubImage2D(hdr_depth_copy, 0, 0, 0, 0, 0, window_width, window_height);
        gl_state.bind_texture(TextureSlot::SCREEN_DEPTH, hdr_depth_copy);

        gl_state.bind_texture(TextureSlot::SIMPLEX_GRADIENT, simplex_gradient->id);
        gl_state.bind_texture(TextureSlot::SIMPLEX_VALUE, simplex_value->id);
        gl_state.bind_texture(TextureSlot::WATER_COLLIDER, water_collider_buffers[water_collider_final_fbo]);
    
        auto model = createModelMatrix(q.water->position, glm::quat(), q.water->scale);
        auto mvp = camera.vp * model;
        glUniformMatrix4fv(Shaders::water.uniform("mvp"), 1, GL_FALSE, &mvp[0][0]);
        glUniformMatrix4fv(Shaders::water.uniform("model"), 1, GL_FALSE, &model[0][0]);
        glUniform4fv(Shaders::water.uniform("shallow_color"), 1, &q.water->shallow_color[0]);
        glUniform4fv(Shaders::water.uniform("deep_color"), 1, &q.water->deep_color[0]);
        glUniform4fv(Shaders::water.uniform("foam_color"), 1, &q.water->foam_color[0]);

        if (water_grid.complete) {
            gl_state.bind_vao(water_grid.vao);
            glDrawElements(water_grid.draw_mode, water_grid.draw_count[0], water_grid.draw_type, 
                            (GLvoid*)(sizeof(*water_grid.indices)*water_grid.draw_start[0]));
        }

        gl_state.set_blend_mode(GlBlendMode::OVERWRITE);
    }
    
    if (do_msaa) {
        resolveMultisampleHdrBuffer();
    }
}

// Since shadow buffer wont need to be bound otherwise just combine these operations
void drawRenderQueueShadows(const RenderQueue& q) {
    gl_state.bind_framebuffer(shadow_fbo);
    gl_state.add_flags(GlFlags::DEPTH_WRITE);
    gl_state.bind_viewport(shadow_size, shadow_size);
    glClear(GL_DEPTH_BUFFER_BIT);

    writeShadowVpsUbo();

    // Only calculate shadows for opaque objects, @todo partial shadows with transparency?
    for (const auto& ri : q.opaque_items) {
        drawRenderItem(ri, nullptr, true);
    }
}

void drawPost(const Camera &camera){
    // Draw screen space quad so clearing is unnecessary
    //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    gl_state.remove_flags(GlFlags::DEPTH_READ | GlFlags::DEPTH_WRITE);

    Shaders::post.set_macro("BLOOM", do_bloom);
    gl_state.bind_program(Shaders::post.program());

    glUniform2f(Shaders::post.uniform("resolution"), window_width, window_height);
    glUniform1f(Shaders::post.uniform("exposure"), exposure);

    if (do_msaa) {
        gl_state.bind_texture(TextureSlot::SCREEN_COLOR, hdr_buffer_resolve_multisample);
    } else {
        gl_state.bind_texture(TextureSlot::SCREEN_COLOR, hdr_buffer);
    }

    gl_state.bind_texture(TextureSlot::BLOOM, bloom_mip_infos[0].texture);

    if (do_msaa) // The resolving step copies the depth
        gl_state.bind_texture(TextureSlot::SCREEN_DEPTH, hdr_depth_copy);
    else
        gl_state.bind_texture(TextureSlot::SCREEN_DEPTH, hdr_depth);

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

    drawQuad();
}

// convert HDR cubemap environment map to scaled down irradiance cubemap
// @note that out_tex should be an empty texture
void convoluteIrradianceFromCubemap(Texture *in_tex, Texture *out_tex, GLint format) {
    out_tex->resolution = glm::ivec2(32, 32); // @hardcoded

    GLuint FBO, RBO;
    glGenFramebuffers(1, &FBO);
    glGenRenderbuffers(1, &RBO);

    gl_state.bind_framebuffer(FBO);
    gl_state.bind_renderbuffer(RBO);

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

    gl_state.bind_texture(0, in_tex->id, GL_TEXTURE_CUBE_MAP);

    // Create output cubemap
    glGenTextures(1, &out_tex->id);
    gl_state.bind_texture(1, out_tex->id, GL_TEXTURE_CUBE_MAP);
    for (unsigned int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, out_tex->resolution.x, out_tex->resolution.y, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    gl_state.bind_program(Shaders::diffuse_convolution.program());

    gl_state.bind_viewport(out_tex->resolution.x, out_tex->resolution.y);
    gl_state.bind_framebuffer(FBO);
    for (unsigned int i = 0; i < 6; ++i) {
        auto vp = projection * glm::mat4(glm::mat3(views[i])); // removes translation
        glUniformMatrix4fv(Shaders::diffuse_convolution.uniform("vp"), 1, GL_FALSE, &vp[0][0]);
        
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, out_tex->id, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        drawCube();
    }

    gl_state.bind_framebuffer(GL_FALSE);
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

    gl_state.bind_framebuffer(FBO);
    gl_state.bind_renderbuffer(RBO);

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

    gl_state.bind_texture(0, in_tex->id, GL_TEXTURE_CUBE_MAP);

    // Create output cubemap
    glGenTextures(1, &out_tex->id);
    gl_state.bind_texture(1, out_tex->id, GL_TEXTURE_CUBE_MAP);
    for (unsigned int i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, out_tex->resolution.x, out_tex->resolution.y, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenerateMipmap(GL_TEXTURE_CUBE_MAP); // Since we store different roughnesses in mip maps

    gl_state.bind_program(Shaders::specular_convolution.program());

    float texelSphericalArea = 4.0 * PI / (6.0 * in_tex->resolution[0] * in_tex->resolution[0]);
    glUniform1f(Shaders::specular_convolution.uniform("texelSphericalArea"), texelSphericalArea);

    glm::ivec2 mip_resolution = out_tex->resolution;
    for (unsigned int mip = 0; mip < MAX_SPECULAR_MIP; ++mip) {
        gl_state.bind_viewport(mip_resolution.x, mip_resolution.y);
        
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

    gl_state.bind_framebuffer(GL_FALSE);
    glDeleteFramebuffers(1, &FBO);
    glDeleteRenderbuffers(1, &RBO);
}

bool createEnvironmentFromCubemap(Environment& env, AssetManager &asset_manager, const std::string& path, GLint format) {
    env.skybox = asset_manager.getTexture(path);
    if (!env.skybox) {
        env.skybox = asset_manager.createTexture(path);

        std::array<std::string, FACE_NUM_FACES> paths = {
            path + "/px.hdr", path + "/nx.hdr",
            path + "/py.hdr", path + "/ny.hdr",
            path + "/pz.hdr", path + "/nz.hdr" };
        if (!asset_manager.loadCubemapTexture(env.skybox, paths, format, GL_REPEAT, true)) {
            std::cerr << "Error loading cubemap\n";
            return false;
        }
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

    gl_state.bind_texture(0, tex->id, GL_TEXTURE_3D);
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

    tex->format = GL_RGBA;
    tex->complete = true;
    return tex;
}