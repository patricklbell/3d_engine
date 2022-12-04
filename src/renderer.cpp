#include "renderer.hpp"

GlState gl_state;
bool GlState::bind_vao(GLuint to_bind) {
    if (to_bind != vao) {
        glBindVertexArray(to_bind);
        vao = to_bind;
        return true;
    }
    return false;
}

bool GlState::bind_program(GLuint to_bind) {
    if (to_bind != program) {
        glUseProgram(to_bind);
        program = to_bind;
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

bool GlState::bind_texture(uint64_t slot, GLuint to_bind, GLenum type) {
    assert(slot < MAX_TEXTURE_SLOTS);

    if (textures[slot] != to_bind) {
        if (GL_TEXTURE0 + slot != active_texture) {
            glActiveTexture(GL_TEXTURE0 + slot);
            active_texture = GL_TEXTURE0 + slot;
        }
        glBindTexture(type, to_bind);
        textures[slot] = to_bind;
        return true;
    }
    return false;
}
bool GlState::bind_texture(TextureSlot slot, GLuint to_bind, GLenum type) {
    return bind_texture((uint64_t)slot, to_bind, type);
}

void GlState::bind_framebuffer(GLuint to_bind, GlBufferFlags flags) {
    if (!!(flags & GlBufferFlags::READ) && to_bind != read_framebuffer) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, to_bind);
        read_framebuffer = to_bind;
    }
    if (!!(flags & GlBufferFlags::WRITE) && to_bind != write_framebuffer) {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, to_bind);
        write_framebuffer = to_bind;
    }
}
bool GlState::bind_renderbuffer(GLuint to_bind) {
    if (to_bind != renderbuffer) {
        glBindRenderbuffer(GL_RENDERBUFFER, to_bind);
        renderbuffer = to_bind;
        return true;
    }
    return false;
}