#ifndef SHADER_HPP
#define SHADER_HPP

GLuint load_shader(const char * vertex_fragment_file_path);
GLuint bind_shader(const GLuint &programID);
GLuint load_geometry_shader(const char * path);
GLuint load_dir_light_shader(const char * path);
GLuint load_point_light_shader(const char * path);
GLuint load_post_shader(const char * path);

extern struct GeometryShadeUniforms{
    GLuint mvp, 
} geometry_shader_uniforms

#endif
