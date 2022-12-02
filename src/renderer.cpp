#include "renderer.hpp"

GlState gl_state;
bool GlState::bind_vao(GLuint desired) {
    if (desired != vao) {
        glBindVertexArray(desired);
        vao = desired;
        return true;
    }
    return false;
}

bool GlState::bind_program(GLuint desired) {
    if (desired != program) {
        glUseProgram(desired);
        program = desired;
        return true;
    }
    return false;
}

static void enable_glflags(GlFlags enable) {
    if (!!enable) {
        if (!!(enable & GlFlags::CULL)) {
            glEnable(GL_CULL_FACE);
        }
        if (!!(enable & GlFlags::ALPHA_COVERAGE)) {
            glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        }
        if (!!(enable & GlFlags::DEPTH_READ)) {
            glEnable(GL_DEPTH_TEST);
        }
        if (!!(enable & GlFlags::DEPTH_WRITE)) {
            glDepthMask(GL_TRUE);
        }
        if (!!(enable & GlFlags::BLEND)) {
            glEnable(GL_BLEND);
        }
    }
}
static void disable_glflags(GlFlags disable) {
    if (!!(disable)) {
        if (!!(disable & GlFlags::CULL)) {
            glDisable(GL_CULL_FACE);
        }
        if (!!(disable & GlFlags::ALPHA_COVERAGE)) {
            glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
        }
        if (!!(disable & GlFlags::DEPTH_READ)) {
            glDisable(GL_DEPTH_TEST);
        }
        if (!!(disable & GlFlags::DEPTH_WRITE)) {
            glDepthMask(GL_FALSE);
        }
        if (!!(disable & GlFlags::BLEND)) {
            glDisable(GL_BLEND);
        }
    }
}

bool GlState::set_flags(GlFlags desired) {
    auto enable = (flags ^ desired) & desired;
    auto disable = (flags ^ desired) & flags;

    enable_glflags(enable);
    disable_glflags(disable);

    flags = desired;

    return !!(enable | disable);
}

bool GlState::add_flags(GlFlags add) {
    auto enable = (flags ^ add) & add;

    enable_glflags(enable);

    flags = (enable | flags);
    return !!enable;
}

bool GlState::remove_flags(GlFlags remove) {
    auto disable = flags & remove;

    disable_glflags(disable);

    flags = disable ^ flags;
    return !!disable;
}

bool GlState::bind_viewport(int x, int y, int w, int h) {
    if (viewport.x != x || viewport.y != y || viewport.z != w || viewport.w != h) {
        glViewport(x, y, w, h);
        glScissor(x, y, w, h);
        viewport = glm::ivec4(x, y, w, h);;
        return true;
    }
    return false;
}

bool GlState::bind_viewport(int w, int h) {
    return bind_viewport(0, 0, w, h);
}