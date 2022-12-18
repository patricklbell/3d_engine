#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <cstdint>
#include <array>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "assets.hpp"
#include "scoped_enum_flag.hpp"

enum class GlFlags : uint64_t {
    NONE = 0,
    CULL = 1 << 0,
    ALPHA_COVERAGE = 1 << 1,
    DEPTH_READ = 1 << 2,
    DEPTH_WRITE = 1 << 3,
    BLEND = 1 << 4,
    ALL = CULL | ALPHA_COVERAGE | DEPTH_READ | DEPTH_WRITE | BLEND,
};
SCOPED_ENUM_FLAG(GlFlags);

enum class GlBufferFlags : uint64_t {
    READ        = 1 << 0,
    WRITE       = 1 << 1,
    READ_WRITE  = READ | WRITE,
};
SCOPED_ENUM_FLAG(GlBufferFlags);

struct GlState {
    bool init();

    bool bind_vao(GLuint to_bind);
    bool bind_program(GLuint to_bind);
    bool bind_viewport(int x, int y, int w, int h);
    bool bind_viewport(int w, int h);
    bool bind_texture(uint64_t slot, GLuint to_bind, GLenum type = GL_TEXTURE_2D);
    bool bind_texture(TextureSlot slot, GLuint to_bind, GLenum type = GL_TEXTURE_2D);
    bool bind_texture_any(GLuint to_bind, GLenum type = GL_TEXTURE_2D);

    void bind_framebuffer(GLuint to_bind, GlBufferFlags flags = GlBufferFlags::READ_WRITE);
    bool bind_renderbuffer(GLuint to_bind);

    bool set_flags(GlFlags desired);
    bool add_flags(GlFlags add);
    bool remove_flags(GlFlags remove);

    bool check_errors(std::string_view file, const int line, std::string_view function);

    static constexpr int MAX_TEXTURE_SLOTS = 32;
    GLuint textures[MAX_TEXTURE_SLOTS] = { GL_FALSE };
    GLenum active_texture = GL_TEXTURE0;
    GLuint read_framebuffer = GL_FALSE, write_framebuffer = GL_FALSE;
    GLuint renderbuffer = GL_FALSE;
    GlFlags flags = GlFlags::ALL;
    GLuint vao = GL_FALSE;
    GLuint program = GL_FALSE;
    glm::ivec4 viewport = glm::ivec4(0);
    std::string version, vendor, renderer, glsl_version;
};
extern GlState gl_state;

struct RenderItem {
    Material* mat;
    uint64_t submesh_i;
    Mesh* mesh;
    AABB* aabb;

    GlFlags flags;

    glm::mat4x4 model;
    std::array<glm::mat4, MAX_BONES>* bone_matrices = nullptr;
    bool draw_shadow = true;
    bool culled = false;
};

struct WaterEntity;
struct RenderQueue {
    std::vector<RenderItem> opaque_items;
    std::vector<RenderItem> transparent_items;
    WaterEntity* water = nullptr; // @todo Decouple entities from rendering
};

#endif // RENDERER_HPP