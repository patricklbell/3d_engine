#ifndef SHADER_HPP
#define SHADER_HPP

#include <string>

#include <GLFW/glfw3.h>

GLuint loadShader(std::string  vertex_fragment_file_path);
void loadGeometryShader(std::string path);
void loadPointLightShader(std::string path);
void loadPostShader(std::string path);
void deleteShaderPrograms();

namespace shader {
    enum TYPE {
        GEOMETRY = 0,
        POINT,
        POST,
        NUM_TYPES,
    };

    extern GLuint geometry;
    extern struct GeometryUniforms {
        GLuint mvp, model; 
    } geometry_uniforms;

    extern GLuint post;
    extern struct PostUniforms {
        GLuint screen_size, light_color, light_direction, camera_position;
    } post_uniforms;

    extern GLuint point;
    extern struct PointUniforms {
        GLuint screen_size, lights, camera_position;
    } point_uniforms;
}

#endif
