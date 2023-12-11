#ifndef ENGINE_GLOBALS_HPP
#define ENGINE_GLOBALS_HPP

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

extern std::string exepath;

extern float true_time_warp, physics_time_warp;
extern bool true_time_pause;

extern GLFWwindow* window;

extern AssetManager global_assets;

struct ThreadPool;
extern ThreadPool *global_thread_pool;

extern SoLoud::Soloud soloud;

#endif // ENGINE_GLOBALS_HPP