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
#include <random>

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
#include "utilities.hpp"
#include "shader.hpp"
#include "texture.hpp"
#include "graphics.hpp"
#include "controls.hpp"
#include "editor.hpp"
#include "assets.hpp"
#include "entities.hpp"
#include "level.hpp"
#include "game_behaviour.hpp"

// Defined in globals.hpp
std::string glsl_version;
std::string exepath;
glm::vec3 sun_color;
glm::vec3 sun_direction;
AssetManager global_assets;
std::string GL_version, GL_vendor, GL_renderer;
std::string level_path = "";
ThreadPool *global_thread_pool;
bool playing = false;
bool has_played = false;
SoLoud::Soloud soloud;

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
    //glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

    glfwSetScrollCallback(window, windowScrollCallback);
    glfwGetWindowSize(window, &window_width, &window_height);
    glfwSetWindowSizeCallback(window, windowSizeCallback);

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSwapInterval(0);

    // @todo MSAA with different levels
    glDisable(GL_MULTISAMPLE);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);  

    GL_version = std::string((char*)glGetString(GL_VERSION));
    GL_vendor = std::string((char*)glGetString(GL_VENDOR));
    GL_renderer = std::string((char*)glGetString(GL_RENDERER));
    std::cout << "OpenGL Info:\nVersion: \t" << GL_version << "\nVendor: \t" << GL_vendor << "\nRenderer: \t" << GL_renderer << "\n";

    soloud.init();
    if(soloud.getBackendString() == NULL) {
        std::cout << "Soloud failed to initialize\n";
    } else {
        std::cout << "Soloud backend string: " << soloud.getBackendString() << "\n";
    }

#if DO_MULTITHREAD
    ThreadPool thread_pool;
    thread_pool.start();
    global_thread_pool = &thread_pool;
#endif

    initDefaultMaterial(global_assets);
    initGraphicsPrimitives(global_assets);
    initShadowFbo();
    initAnimationUbo();
    initWaterColliderFbo();
    initHdrFbo();
    if(graphics::do_bloom){
        initBloomFbo();
    }
    initEditorGui(global_assets);
    initEditorControls();

    // @note camera instanciation uses sun direction to create shadow view
    sun_direction = glm::vec3(-0.7071067811865475, -0.7071067811865475, 0);
    sun_color = 5.0f*glm::vec3(0.941, 0.933, 0.849);

    createDefaultCamera(editor_camera);
    createDefaultCamera(level_camera);
    level_camera.state = Camera::TYPE::STATIC;
    updateShadowVP(editor_camera);

    // 
    // Load shaders
    //
    initGlobalShaders();

    AssetManager asset_manager;
    EntityManager *entity_manager = &level_entity_manager;

    // Load background sample
    auto bg_music = asset_manager.createAudio("data/audio/time.wav");
    if (bg_music->wav_stream.load(bg_music->handle.c_str()) != SoLoud::SO_NO_ERROR)
        std::cout << "Error loading wav\n";
    bg_music->wav_stream.setLooping(1);                          // Tell SoLoud to loop the sound
    int handle1 = soloud.play(bg_music->wav_stream);             // Play it
    soloud.setVolume(handle1, 0.5f);            // Set volume; 1.0f is "normal"
    soloud.setPan(handle1, -0.2f);              // Set pan; -1 is left, 1 is right
    soloud.setRelativePlaySpeed(handle1, 1.0f); // Play a bit slower; 1.0f is normal

    // level_path = "data/levels/test.level";
    // loadLevel(entity_manager, asset_manager, level_path, level_camera);
    // editor_camera = level_camera;
    
    auto veg = (VegetationEntity*)entity_manager->createEntity(VEGETATION_ENTITY);
    veg->mesh = &graphics::seaweed;
    veg->texture = asset_manager.createTexture("data/textures/extern/Leaves/Leaves_Pine_Texture.png");
    asset_manager.loadTexture(veg->texture, veg->texture->handle, GL_RGBA);

    // auto anim_entity = (AnimatedMeshEntity*)entity_manager->createEntity(ANIMATED_MESH_ENTITY);
    // anim_entity->mesh    = asset_manager.createMesh("data/mesh/dancing_vampire.mesh");
    // anim_entity->animesh = asset_manager.createAnimatedMesh("data/anim/dancing_vampire.anim");
    // asset_manager.loadMeshFile(anim_entity->mesh, "data/mesh/dancing_vampire.mesh");
    // asset_manager.loadAnimationFile(anim_entity->animesh, "data/anim/dancing_vampire.anim");
    // anim_entity->play("Armature|dance", 0.0, 1.0, true);

    std::array<std::string, 6> skybox_paths = { "data/textures/simple_skybox/0006.png", "data/textures/simple_skybox/0002.png",
                                                "data/textures/simple_skybox/0005.png", "data/textures/simple_skybox/0004.png",
                                                "data/textures/simple_skybox/0003.png", "data/textures/simple_skybox/0001.png" };
    auto skybox = global_assets.createTexture("skybox");
    global_assets.loadCubemapTexture(skybox, skybox_paths);

#ifndef NDEBUG 
    checkGLError("Pre-loop");
#endif

    double last_time = glfwGetTime();
    double last_filesystem_hotswap_check = last_time;
    uint64_t frame_num = 0;
    window_resized = true;
    do {
        
        double current_time = glfwGetTime();
        float true_dt = current_time - last_time;
        last_time = current_time;
        static const float dt = 1.0/60.0;

        // @todo
        Camera* camera_ptr = &editor_camera;
        if (playing) {
            camera_ptr = &game_camera;
        } else if (editor::use_level_camera) {
            camera_ptr = &level_camera;
        }
        Camera& camera = *camera_ptr;

        if (window_resized){
            updateCameraProjection(camera);
            
            initHdrFbo(true);

            if(graphics::do_bloom){
                initBloomFbo(true);
            }
        }

        // Hotswap shader files
        if(current_time - last_filesystem_hotswap_check >= 1.0){
            last_filesystem_hotswap_check = current_time;
            updateGlobalShaders();
        }

        entity_manager->tickAnimatedMeshes(true_dt);
        updateGameEntities(true_dt);

        bindDrawShadowMap(*entity_manager, camera);
        bindHdr();
        clearFramebuffer();
        drawUnifiedHdr(*entity_manager, skybox, camera);

        if (playing) {
            handleGameControls(entity_manager, asset_manager, true_dt);
        }
        else {
            handleEditorControls(entity_manager, asset_manager, true_dt);
        }
        
        if (graphics::do_bloom) {
            blurBloomFbo();
        }
        bindBackbuffer();
        drawPost(skybox, camera);
        
        if (!playing) {
            drawEditorGui(*entity_manager, asset_manager);
        }
        else {
            drawGameGui(*entity_manager, asset_manager);
        }
        // Swap backbuffer with front buffer
        glfwSwapBuffers(window);
        window_resized = false;
        glfwPollEvents();

        entity_manager->propogateChanges();

#ifndef NDEBUG
        checkGLError("Main loop");
        // Should catch any other errors but the message may be less descriptive
        static GLenum code;
        static const GLubyte* string;
        code = glGetError();
        if(code != GL_NO_ERROR){
            string = gluErrorString(code);
            std::cerr << "<--------------------OpenGL ERROR-------------------->\n" << string << "\n";
        }
        frame_num++;
#endif
    } // Check if the ESC key was pressed or the window was closed
    while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS && glfwWindowShouldClose(window) == 0 );

    // Clean up SoLoud
    soloud.deinit();

#if DO_MULTITHREAD
    thread_pool.stop();
#endif

    // Close OpenGL window and terminate GLFW
    glfwTerminate();

    return 0;
}


