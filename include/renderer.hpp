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

struct GlState {
    bool bind_vao(GLuint desired);
    bool bind_program(GLuint desired);
    bool bind_viewport(int x, int y, int w, int h);
    bool bind_viewport(int w, int h);

    bool set_flags(GlFlags desired);
    bool add_flags(GlFlags add);
    bool remove_flags(GlFlags remove);

    GlFlags flags = GlFlags::ALL;
    GLuint vao = GL_FALSE;
    GLuint program = GL_FALSE;
    glm::ivec4 viewport = glm::ivec4(0);
};
extern GlState gl_state;

struct RenderItem {
    Material* mat;
    uint64_t submesh_i;
    Mesh* mesh;

    GlFlags flags;

    glm::mat4x4 model;
    std::array<glm::mat4, MAX_BONES>* bone_matrices = nullptr;
};

struct WaterEntity;
struct RenderQueue {
    std::vector<RenderItem> opaque_items;
    std::vector<RenderItem> transparent_items;
    WaterEntity* water = nullptr; // @todo Decouple entities from rendering
};

#endif // RENDERER_HPP