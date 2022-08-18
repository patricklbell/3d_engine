#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <vector>
#include <stack>
#include <array>
#include <map>
#include <filesystem>
#include <thread>

// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <GLFW/glfw3.h>
#ifdef _WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <ShellScalingApi.h>
#endif
GLFWwindow* window;

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "globals.hpp"
#include "shader.hpp"
#include "texture.hpp"
#include "utilities.hpp"
#include "graphics.hpp"
#include "controls.hpp"
#include "editor.hpp"
#include "assets.hpp"
#include "entities.hpp"

// Defined in globals.hpp
std::string glsl_version;
std::string exepath;
glm::vec3 sun_color;
glm::vec3 sun_direction;
AssetManager global_assets;
std::string GL_version, GL_vendor, GL_renderer;
std::string level_path = "";
ThreadPool *global_thread_pool;

int main() {
    exepath = getexepath();

    // Initialise GLFW
    if( !glfwInit() )
    {
        std::cerr << "Failed to initialize GLFW\n";
        getchar();
        return -1;
    }
    
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glsl_version = "#version 430\n";
#ifdef __APPLE__
    // GL 4.3 + GLSL 430
    glsl_version = "#version 430\n";
    glfwWindowHint( // required on Mac OS
        GLFW_OPENGL_FORWARD_COMPAT,
        GL_TRUE
    );
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#elif __linux__
    // GL 4.3 + GLSL 430
    glsl_version = "#version 430\n";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#elif _WIN32
    // GL 4.3 + GLSL 430
    glsl_version = "#version 430\n";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#endif

#if __APPLE__
    // to prevent 1200x800 from becoming 2400x1600
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
#endif
    //std::ios_base::sync_with_stdio(false);

    // Open a window and create its OpenGL context
    window = glfwCreateWindow(1024, 700, "Window", NULL, NULL);
    if( window == NULL ) {
        std::cerr << "Failed to open GLFW window.\n";
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);

    // Initialize GLEW
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW.\n";
        glfwTerminate();
        return 1;
    }

    // Ensure we can capture the escape key being pressed below
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

    glfwSetScrollCallback(window, windowScrollCallback);
    glfwGetWindowSize(window, &window_width, &window_height);
    glfwSetWindowSizeCallback(window, windowSizeCallback);

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSwapInterval(0);

    // We implement FXAA so multisampling should be unnecessary
    glDisable(GL_MULTISAMPLE);

    GL_version = std::string((char*)glGetString(GL_VERSION));
    GL_vendor = std::string((char*)glGetString(GL_VENDOR));
    GL_renderer = std::string((char*)glGetString(GL_RENDERER));
    std::cout << "OpenGL Info:\nVersion: \t" << GL_version << "\nVendor: \t" << GL_vendor << "\nRenderer: \t" << GL_renderer << "\n";

#if DO_MULTITHREAD
    ThreadPool thread_pool;
    thread_pool.start();
    global_thread_pool = &thread_pool;
#endif

    initDefaultMaterial(global_assets);
    initGraphicsPrimitives(global_assets);
    initShadowFbo();
    initHdrFbo();
    if(shader::unified_bloom){
        initBloomFbo();
    }
    initEditorGui(global_assets);
    initEditorControls();

    // @note camera instanciation uses sun direction to create shadow view
    sun_direction = glm::vec3(-0.7071067811865475, -0.7071067811865475, 0);
    sun_color = 5.0f*glm::vec3(0.941, 0.933, 0.849);

    Camera camera;
    createDefaultCamera(camera);
    updateShadowVP(camera);

    // 
    // Load shaders
    //
    // Map for last update time for hotswaping files
    std::filesystem::file_time_type empty_file_time;

    struct ShaderData {
        std::string path;
        shader::TYPE type;
        std::filesystem::file_time_type update_time;
    };

    std::vector<ShaderData> shader_update_times {
        {"data/shaders/null.gl", shader::TYPE::NULL_SHADER, empty_file_time},
        {"data/shaders/unified.gl", shader::TYPE::UNIFIED_SHADER, empty_file_time},
        {"data/shaders/water.gl", shader::TYPE::WATER_SHADER, empty_file_time},
        {"data/shaders/gaussian_blur.gl", shader::TYPE::GAUSSIAN_BLUR_SHADER, empty_file_time},
        {"data/shaders/post.gl", shader::TYPE::POST_SHADER, empty_file_time},
        {"data/shaders/debug.gl", shader::TYPE::DEBUG_SHADER, empty_file_time},
        //{"data/shaders/skybox.gl", shader::TYPE::SKYBOX_SHADER, empty_file_time},
    };

    // Fill in with correct file time and actually load
    for (auto &shader: shader_update_times) {
        if(std::filesystem::exists(shader.path)) 
            shader.update_time = std::filesystem::last_write_time(shader.path);

        loadShader(shader.path, shader.type);
    }

    AssetManager asset_manager;
    EntityManager entity_manager;

    //level_path = "data/levels/water_test.level";
    //loadLevel(entity_manager, asset_manager, level_path);

    //auto t_e = new TerrainEntity();
    //t_e->texture = createTextureAsset(assets, "data/textures/iceland_heightmap.png");
    //entity_manager.setEntity(entity_manager.getFreeId().i, t_e);

    std::array<std::string,6> skybox_paths = {"data/textures/cloudy/bluecloud_ft.jpg", "data/textures/cloudy/bluecloud_bk.jpg",
                                              "data/textures/cloudy/bluecloud_up.jpg", "data/textures/cloudy/bluecloud_dn.jpg",
                                              "data/textures/cloudy/bluecloud_rt.jpg", "data/textures/cloudy/bluecloud_lf.jpg"};
    auto skybox = global_assets.createTexture("skybox");
    AssetManager::loadCubemapTexture(skybox, skybox_paths);
#ifndef NDEBUG 
    checkGLError("Pre-loop");
#endif

    double last_time = glfwGetTime();
    double last_filesystem_hotswap_check = last_time;
    window_resized = true;
    do {
        double current_time = glfwGetTime();
        float true_dt = current_time - last_time;
        last_time = current_time;
        static const float dt = 1.0/60.0;

        if (window_resized){
            updateCameraProjection(camera);
            initHdrFbo(true);

            if(shader::unified_bloom){
                initBloomFbo(true);
            }
        }

        // Hotswap shader files
        if(current_time - last_filesystem_hotswap_check >= 1.0){
            last_filesystem_hotswap_check = current_time;

            for (auto &shader: shader_update_times) {
                if(std::filesystem::exists(shader.path) && shader.update_time != std::filesystem::last_write_time(shader.path)){
                    shader.update_time = std::filesystem::last_write_time(shader.path);

                    loadShader(shader.path, shader.type);
                } 
            }
        }
        bindDrawShadowMap(entity_manager, camera);
        bindHdr();
        clearFramebuffer(glm::vec4(0.1,0.1,0.1,1.0));
        drawUnifiedHdr(entity_manager, skybox, camera);

        handleEditorControls(camera, entity_manager, true_dt);

        int blur_buffer_index = 0;
        if(shader::unified_bloom){
            blur_buffer_index = blurBloomFbo();
        }

        bindBackbuffer();
        drawPost(blur_buffer_index, skybox, camera);

        drawEditorGui(camera, entity_manager, asset_manager);
        // Swap backbuffer with front buffer
        glfwSwapBuffers(window);
        window_resized = false;
        glfwPollEvents();

        entity_manager.propogateChanges();

#ifndef NDEBUG
        checkGLError("main loop");
        // Should catch any other errors but the message may be less descriptive
        static GLenum code;
        static const GLubyte* string;
        code = glGetError();
        if(code != GL_NO_ERROR){
            string = gluErrorString(code);
            std::cerr << "<--------------------OpenGL ERROR-------------------->\n" << string << "\n";
        }
#endif
    } // Check if the ESC key was pressed or the window was closed
    while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS && glfwWindowShouldClose(window) == 0 );

#if DO_MULTITHREAD
    thread_pool.stop();
#endif

    deleteShaderPrograms();    

    // Close OpenGL window and terminate GLFW
    glfwTerminate();

    return 0;
}


