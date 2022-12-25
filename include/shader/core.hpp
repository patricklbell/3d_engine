#ifndef SHADER_CORE_HPP
#define SHADER_CORE_HPP

#include <unordered_map>
#include <filesystem>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

struct Shader {
    ~Shader();
    bool load_file(std::string_view path);
    bool load_file_compile(std::string_view path, std::string_view _prepend = "");
    bool ready();
    bool compile(std::string_view _prepend = "");
    void clear_loaded();
    bool update_from_dependencies();
    constexpr GLuint program() { return active_program; }
    bool Shader::activate_macros(); // Returns true if recompile was necessary
    bool set_macro(std::string_view macro, bool value, bool activate = true);
    GLuint uniform(std::string_view name);

    std::string handle = "";

    enum class Type : uint64_t {
        VERTEX = 0,
        FRAGMENT,
        GEOMETRY,
        COMPUTE,
        TESSELLATION_CONTROL,
        TESSELLATION_EVALUATION,
        NUM,
        NONE,
    };
    struct FileDependency {
        std::string path;
        std::filesystem::file_time_type last_write_time;
    };

private:
    bool file_loaded = false;
    std::unordered_map<std::string, FileDependency> dependencies;
    std::unordered_map<Type, std::string> type_to_chunk;

    std::unordered_map <uint64_t, std::unordered_map<std::string, GLuint>> programs_uniforms;
    std::string prepend = "";
    std::unordered_map<std::string, bool> macros;
    std::unordered_map<uint64_t, GLuint> programs;

    uint64_t active_hash = 0;
    GLuint active_program = GL_FALSE;
};


#endif // SHADER_CORE_HPP