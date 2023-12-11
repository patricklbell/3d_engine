#include "renderer.hpp"
#include <iostream>

bool GlState::init() {
    // GL 4.3 + GLSL 430
    glsl_version = "#version 430\n";

    version = std::string((char*)glGetString(GL_VERSION));
    vendor = std::string((char*)glGetString(GL_VENDOR));
    renderer = std::string((char*)glGetString(GL_RENDERER));

    std::cout << "OpenGL Info:\nVersion: \t" << version << "\nVendor: \t" << vendor << "\nRenderer: \t" << renderer << "\n";

    // Configure gl global state
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_LINE_SMOOTH);
    glCullFace(GL_BACK);
    glPatchParameteri(GL_PATCH_VERTICES, 3);

    // Set clear color, doesn't really matter since we use a skybox anyway
    glClearColor(0.0, 0.0, 0.0, 1.0);

    return !check_errors(__FILE__, __LINE__, __func__);
}

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

bool GlState::set_blend_mode(GlBlendMode mode) {
    if (mode != blend_mode) {
        switch (mode)
        {
        case GlBlendMode::OVERWRITE:
            glBlendFunc(GL_ONE, GL_ZERO);
            break;
        case GlBlendMode::ALPHA:
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case GlBlendMode::REVERSE_ALPHA:
            glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);
            break;
        case GlBlendMode::ADDITIVE:
            glBlendFunc(GL_ONE, GL_ONE);
            break;
        case GlBlendMode::MULTIPLICATIVE:
            glBlendFunc(GL_DST_COLOR, GL_ZERO);
            break;
        }
        blend_mode = mode;
        return true;
    }
    return false;
}

bool GlState::set_cull_mode(GlCullMode mode) {
    if (mode != cull_mode) {
        switch (mode)
        {
        case GlCullMode::BACK:
            glCullFace(GL_BACK);
            break;
        case GlCullMode::FRONT:
            glCullFace(GL_FRONT);
            break;
        }
        cull_mode = mode;
        return true;
    }
    return false;
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
bool GlState::bind_texture_any(GLuint to_bind, GLenum type) {
    if (textures[active_texture - GL_TEXTURE0] != to_bind) {
        glBindTexture(type, to_bind);
        textures[active_texture - GL_TEXTURE0] = to_bind;
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

bool GlState::check_errors(std::string_view file, const int line, std::string_view function) {
    int error_code = (int)glGetError();
    if (error_code == 0)
        return false;

    std::string error = "Unknown error";
    std::string description = "No description";

    if (error_code == GL_INVALID_ENUM)
    {
        error = "GL_INVALID_ENUM";
        description = "An unacceptable value has been specified for an enumerated argument.";
    }
    else if (error_code == GL_INVALID_VALUE)
    {
        error = "GL_INVALID_VALUE";
        description = "A numeric argument is out of range.";
    }
    else if (error_code == GL_INVALID_OPERATION)
    {
        error = "GL_INVALID_OPERATION";
        description = "The specified operation is not allowed in the current state.";
    }
    else if (error_code == GL_STACK_OVERFLOW)
    {
        error = "GL_STACK_OVERFLOW";
        description = "This command would cause a stack overflow.";
    }
    else if (error_code == GL_STACK_UNDERFLOW)
    {
        error = "GL_STACK_UNDERFLOW";
        description = "This command would cause a stack underflow.";
    }
    else if (error_code == GL_OUT_OF_MEMORY)
    {
        error = "GL_OUT_OF_MEMORY";
        description = "There is not enough memory left to execute the command.";
    }
    else if (error_code == GL_INVALID_FRAMEBUFFER_OPERATION)
    {
        error = "GL_INVALID_FRAMEBUFFER_OPERATION";
        description = "The object bound to FRAMEBUFFER_BINDING is not 'framebuffer complete'.";
    }
    else if (error_code == GL_CONTEXT_LOST)
    {
        error = "GL_CONTEXT_LOST";
        description = "The context has been lost, due to a graphics card reset.";
    }
    else if (error_code == GL_TABLE_TOO_LARGE)
    {
        error = "GL_TABLE_TOO_LARGE";
        description = std::string("The exceeds the size limit. This is part of the ") +
            "(Architecture Review Board) ARB_imaging extension.";
    }

    std::cerr << "An internal OpenGL call failed, checked in function '" << function << "' in file " << file << " at line " <<  line << 
        "\nError was '" + error + "' description: " + description + "\n";

    return true;
}