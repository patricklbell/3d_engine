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

#include <controls/behaviour.hpp>
#include <controls/globals.hpp>

#include <camera/globals.hpp>
#include <shader/globals.hpp>

#include "globals.hpp"
#include "graphics.hpp"
#include "utilities.hpp"
#include "texture.hpp"
#include "editor.hpp"
#include "assets.hpp"
#include "entities.hpp"
#include "level.hpp"
#include "game_behaviour.hpp"
#include "lightmapper.hpp"

// Defined in globals.hpp
std::string exepath;
glm::vec3 sun_color;
glm::vec3 sun_direction;
AssetManager global_assets;
std::string GL_version, GL_vendor, GL_renderer;
std::string level_path = "";
ThreadPool *global_thread_pool;
bool playing = false, global_paused = false;
bool has_played = false;
SoLoud::Soloud soloud;
float global_time_warp = 1.0;

static void cleanup() {
    // Clean up SoLoud
    soloud.deinit();

#if DO_MULTITHREAD
    global_thread_pool->stop();
#endif

    // Close OpenGL window and terminate GLFW
    glfwTerminate();
}

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

#ifdef __APPLE__
    // GL 4.3 + GLSL 430
    glfwWindowHint( // required on Mac OS
        GLFW_OPENGL_FORWARD_COMPAT,
        GL_TRUE
    );
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#elif __linux__
    // GL 4.3 + GLSL 430
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#elif _WIN32
    // GL 4.3 + GLSL 430
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

    glfwGetWindowSize(window, &window_width, &window_height);
    glfwSetWindowSizeCallback(window, windowSizeCallback);

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSwapInterval(0);

    // Configure gl global state
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glEnable(GL_SCISSOR_TEST);
    glCullFace(GL_BACK);

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


    // 
    // Load shaders
    //
    if (!Shaders::init()) {
        std::cerr << "Failed to load shaders, you may have the wrong working directory."
            " Updating Opengl or your GPU drivers may also fix the problem.\n";
        cleanup();
        return true;
    }

    // 
    // Load key binding
    //
    Controls::registerCallbacks();
    Controls::editor.loadFromFile("data/editor_controls.txt");
    Controls::game.loadFromFile("data/game_controls.txt");

    initDefaultMaterial(global_assets); // Should only be needed by editor meshes
    initGraphics(global_assets);
    initEditorGui(global_assets);

    // @note camera instanciation uses sun direction to create shadow view
    sun_direction = glm::normalize(glm::vec3(-1, -1, -0.25)); // use z = -2.25 for matching stonewall
    sun_color = 5.0f*glm::vec3(0.941, 0.933, 0.849);

    {
        Frustrum frustrum;
        frustrum.aspect_ratio = (float)window_width / (float)window_height;

        Cameras::editor_camera.set_frustrum(frustrum);
        Cameras::level_camera.set_frustrum(frustrum);
        Cameras::level_camera.state = Camera::TYPE::STATIC;
    }

    /*createEnvironmentFromCubemap(graphics::environment, global_assets,
        { "data/textures/simple_skybox/0006.png", "data/textures/simple_skybox/0002.png",
          "data/textures/simple_skybox/0005.png", "data/textures/simple_skybox/0004.png",
          "data/textures/simple_skybox/0003.png", "data/textures/simple_skybox/0001.png" });*/
    createEnvironmentFromCubemap(graphics::environment, global_assets,
        { "data/textures/stonewall_skybox/px.hdr", "data/textures/stonewall_skybox/nx.hdr",
          "data/textures/stonewall_skybox/py.hdr", "data/textures/stonewall_skybox/ny.hdr",
          "data/textures/stonewall_skybox/pz.hdr", "data/textures/stonewall_skybox/nz.hdr" }, GL_RGB16F);

    AssetManager asset_manager;
    initDefaultMaterial(asset_manager);
    EntityManager *entity_manager = &level_entity_manager;

    // Load background music
    //auto bg_music = asset_manager.createAudio("data/audio/time.wav");
    //if (bg_music->wav_stream.load(bg_music->handle.c_str()) != SoLoud::SO_NO_ERROR)
    //    std::cout << "Error loading wav\n";
    //bg_music->wav_stream.setLooping(1);                          // Tell SoLoud to loop the sound
    //int handle1 = soloud.play(bg_music->wav_stream);             // Play it
    //soloud.setVolume(handle1, 0.5f);            // Set volume; 1.0f is "normal"
    //soloud.setPan(handle1, -0.2f);              // Set pan; -1 is left, 1 is right
    //soloud.setRelativePlaySpeed(handle1, 1.0f); // Play a bit slower; 1.0f is normal

    //level_path = "data/levels/lightmap_test.level";
    //loadLevel(*entity_manager, asset_manager, level_path, Cameras::level_camera);
    Cameras::level_camera.set_position(glm::vec3(1, 1, 0));
    Cameras::level_camera.set_target(glm::vec3(0));
    Cameras::editor_camera = Cameras::level_camera;

#ifndef NDEBUG 
    checkGLError("Pre-loop");
#endif

    double last_time = glfwGetTime();
    double last_filesystem_hotswap_check = last_time;
    uint64_t frame_num = 0;
    window_resized = true;

    RenderQueue render_queue;
    Camera *camera_ptr = nullptr; // The currently active camera
    do {
        double current_time = glfwGetTime();
        double true_dt = (current_time - last_time) * global_time_warp;
        last_time = current_time;
        float dt = 1.0/60.0;
        if (global_paused) {
            true_dt = 0.0f;
            dt = 0.0f;
        }

        // Hotswap shader files every second
        if (current_time - last_filesystem_hotswap_check >= 1.0) {
            last_filesystem_hotswap_check = current_time;
            if (!Shaders::live_update()) {
                std::cerr << "Failed to load shaders, you may have the wrong working directory."
                             " Updating Opengl or your GPU drivers may also fix the problem.\n";
                cleanup();
                return true;
            }
        }
        
        if (window_resized) {
            gl_state.bind_viewport(window_width, window_height);
            float r = (float)window_width / (float)window_height;
            Cameras::level_camera.set_aspect_ratio(r);
            Cameras::game_camera.set_aspect_ratio(r);
            Cameras::editor_camera.set_aspect_ratio(r);

            initHdrFbo(true);

            if(graphics::do_bloom){
                initBloomFbo(true);
            }
        }


        // Handle controls and inputs
        if (playing) {
            handleGameControls(entity_manager, asset_manager, true_dt);
        }
        else {
            handleEditorControls(entity_manager, asset_manager, true_dt);
        }

        // Update entity states and animations
        entity_manager->tickAnimatedMeshes(true_dt);
        if (playing)
            updateGameEntities(true_dt, entity_manager);

        auto old_camera_ptr = camera_ptr;
        camera_ptr = Cameras::get_active_camera();
        Camera& camera = *camera_ptr;

        // Update camera and shadow projections if either the camera changed or it needs updating
        if (camera.update() || old_camera_ptr != camera_ptr)
            updateShadowVP(camera);

        // 
        // Draw entities
        //
        createRenderQueue(render_queue, *entity_manager);

        if (graphics::do_shadows)
            drawRenderQueueShadows(render_queue);
        if (graphics::do_volumetrics)
            computeVolumetrics(frame_num, camera);

        bindHdr();
        clearFramebuffer();
        drawRenderQueue(render_queue, graphics::environment.skybox, graphics::environment.skybox_irradiance, graphics::environment.skybox_specular, camera);

        if (graphics::do_bloom)
            blurBloomFbo(true_dt);
        bindBackbuffer();
        drawPost(camera);

        // 
        // Draw guis
        //
        if (!playing)
            drawEditorGui(*entity_manager, asset_manager);
        else
            drawGameGui(*entity_manager, asset_manager);

        // Swap backbuffer with front buffer
        glfwSwapBuffers(window);

        // 
        // Poll events
        //
        window_resized = false;
        // glfw doesnt send mouse event when mouse/wheel isn't moving @todo
        Controls::delta_mouse_position = glm::dvec2(0); 
        Controls::scroll_offset = glm::dvec2(0);
        glfwPollEvents();

        Controls::editor.update(current_time);
        Controls::game.update(current_time);
        entity_manager->propogateChanges();

#ifndef NDEBUG
        checkGLError("Main loop");
#endif
        frame_num++;
    } while (!Controls::editor.isAction("exit") && !glfwWindowShouldClose(window));

    cleanup();
    return false;
}


