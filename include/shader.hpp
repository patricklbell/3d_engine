#ifndef SHADER_HPP
#define SHADER_HPP

#include <string>

#include <GLFW/glfw3.h>

GLuint loadShader(std::string  vertex_fragment_file_path);
void loadNullShader(std::string path);
void loadGeometryShader(std::string path);
void loadPointShader(std::string path);
void loadDirectionalShader(std::string path);
void loadPostShader(std::string path);
void deleteShaderPrograms();

namespace shader {
    enum TYPE {
        NULL_SHADER = 0,
        GEOMETRY_SHADER,
        POINT_SHADER,
        DIRECTIONAL_SHADER,
        POST_SHADER,
        NUM_SHADER_TYPES,
    };
    extern GLuint null;
    extern struct NullUniforms {
        GLuint mvp; 
    } null_uniforms;

    extern GLuint geometry;
    extern struct GeometryUniforms {
        GLuint mvp, model; 
    } geometry_uniforms;

    extern GLuint post;
    extern struct PostUniforms {
        GLuint screen_size;
    } post_uniforms;

    extern GLuint directional;
    extern struct DirectionalUniforms {
        GLuint screen_size, light_color, light_direction, camera_position;
    } directional_uniforms;


    extern GLuint point;
    extern struct PointUniforms {
        GLuint screen_size, light_position, light_color, attenuation_linear, attenuation_constant, attenuation_exp, camera_position;
    } point_uniforms;
}

#endif
