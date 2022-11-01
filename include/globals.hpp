#ifndef GLOBALS_H
#define GLOBALS_H

// disable Windows.h min max macros
#ifdef _WINDOWS
#define NOMINMAX 
#endif

#include <soloud.h>
#include <soloud_thread.h>
#include <soloud_wav.h>
#include <soloud_wavstream.h>

#include <string>

#include <glm/glm.hpp>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define DO_MULTITHREAD 1
#define ENTITY_COUNT 1000
#define PI           3.14159265359

#include <assets.hpp>

extern GLFWwindow* window;
extern std::string glsl_version;
extern std::string exepath;
extern glm::vec3 sun_color;
extern glm::vec3 sun_direction;
extern glm::vec4 clear_color;
extern bool draw_bloom;
extern AssetManager global_assets;
extern std::string GL_version, GL_vendor, GL_renderer;
extern std::string level_path;
extern bool playing, global_paused;
extern bool has_played;
extern float global_time_warp;

struct ThreadPool;
extern ThreadPool *global_thread_pool;

extern SoLoud::Soloud soloud;

#endif
