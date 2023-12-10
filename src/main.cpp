#include <iostream>
#include <sstream>
#include <thread>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#ifdef _WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <ShellScalingApi.h>
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/detail/type_vec.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

#include <controls/behaviour.hpp>
#include <controls/globals.hpp>

#include <camera/globals.hpp>
#include <shader/globals.hpp>

#include "globals.hpp"
#include "graphics.hpp"
#include "threadpool.hpp"
#include "editor.hpp"
#include "assets.hpp"
#include "entities.hpp"
#include "level.hpp"
#include "game_behaviour.hpp"
#include "physics.hpp"

// Defined in globals.hpp
GLFWwindow* window;
AssetManager global_assets;
ThreadPool *global_thread_pool = nullptr;
std::string exepath;
SoLoud::Soloud soloud;
float true_time_warp = 1.0, physics_time_warp = 1.0;
bool true_time_pause = false;

int cleanup();
bool initialize_glfw();
bool initialize_sound();
EntityManager* get_active_entities();
std::string getexepath();

int main() {
    // 
    // Initialize glfw and open window
    //
    if (!initialize_glfw())
        return cleanup();

    // 
    // Initialize sound engine
    //
    if (!initialize_sound())
        return cleanup();

    // 
    // Initialize opengl
    //
    if (!gl_state.init())
        return cleanup();

    // 
    // Initialize physics engine
    //
    initPhysics();

#if DO_MULTITHREAD
    ThreadPool thread_pool;
    thread_pool.start();
    global_thread_pool = &thread_pool;
#endif

    exepath = getexepath();

    // 
    // Load shaders
    //
    if (!Shaders::init())
        return cleanup();

    // 
    // Load key binding
    //
    Controls::registerCallbacks();
    Controls::editor.loadFromFile("data/editor_controls.txt");
    Controls::game.loadFromFile("data/game_controls.txt");

    initDefaultMaterial(global_assets); // Should only be needed by editor meshes
    initGraphics(global_assets);
    initEditorGui(global_assets);

    AssetManager local_assets;
    initDefaultLevel(loaded_level, local_assets);
    Cameras::update_cameras_for_level();

    // Testing physics
    auto mesh = local_assets.createMesh("cube");
    local_assets.loadMeshFile(mesh, "data/mesh/cube.mesh");

    auto box = (MeshEntity*)loaded_level.entities.createEntity(MESH_ENTITY);
    box->mesh = mesh;
    box->position += glm::vec3(0,5,0);
    box->body_settings = new JPH::BodyCreationSettings(
        new JPH::BoxShape(JPH::RVec3(1,1,1)), 
        JPH::RVec3(0.0, 5.0, 0.0), 
        JPH::Quat::sEulerAngles(JPH::RVec3(0.1,0.3,0.2)), 
        JPH::EMotionType::Dynamic, 
        physics::Layers::MOVING
    );

    auto floor = (MeshEntity*)loaded_level.entities.createEntity(MESH_ENTITY);
    floor->mesh = mesh;
    floor->scale[0][0] *= 10;
    floor->scale[2][2] *= 10;
    floor->body_settings = new JPH::BodyCreationSettings(
        new JPH::BoxShape(JPH::RVec3(10, 1, 10)), 
        JPH::RVec3(0.0, 0.0, 0.0), 
        JPH::Quat::sIdentity(), 
        JPH::EMotionType::Static, 
        physics::Layers::NON_MOVING
    );

    double last_time = glfwGetTime();
    double last_filesystem_hotswap_check = last_time;
    uint64_t frame_num = 0;
    window_resized = true;

    RenderQueue render_queue;
    LightQueue light_queue;
    Camera *camera_ptr = nullptr; // The currently active camera
    EntityManager* entities_ptr = nullptr; // The currently active entities
    do {
        double current_time = glfwGetTime();
        double true_dt = true_time_pause ? 0.0 : (current_time - last_time) * true_time_warp;
        last_time = current_time;

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
        
        if (window_resized && window_height > 0) {
            gl_state.bind_viewport(window_width, window_height);
            float r = (float)window_width / (float)window_height;
            loaded_level.camera.set_aspect_ratio(r);
            gamestate.level.camera.set_aspect_ratio(r);
            Editor::editor_camera.set_aspect_ratio(r);

            initHdrFbo(true);
            if(graphics::do_bloom)
                initBloomFbo(true);
        }

        entities_ptr = get_active_entities();

        // Handle controls and inputs, @note this involves changing the active entities
        if (gamestate.is_active)
            handleGameControls(entities_ptr, local_assets, true_dt);
        else 
            handleEditorControls(entities_ptr, local_assets, true_dt);

        EntityManager& entities = *entities_ptr;

        // Update entity states and animations
        tickEntities(entities, true_dt, gamestate.is_active);
        if (gamestate.is_active) {
            // Step the world
            float dt = 1.0/60.0 * physics_time_warp;
            physics::system->Update(dt, 1, physics::temp_allocator, physics::job_system);
        }

        auto old_camera_ptr = camera_ptr;
        camera_ptr = Cameras::get_active_camera();
        Camera& camera = *camera_ptr;

        // Update camera and shadow projections if either the camera changed or it needs updating
        if (camera.update() || old_camera_ptr != camera_ptr)
            updateShadowVP(camera, loaded_level.environment.sun_direction);

        // 
        // Draw entities
        //
        createLightQueue(light_queue, entities);
        createRenderQueue(render_queue, entities);
        addLightsToRenderQueue(render_queue, light_queue);

        if (graphics::do_shadows)
            drawRenderQueueShadows(render_queue);
        if (graphics::do_volumetrics)
            computeVolumetrics(loaded_level.environment, light_queue, frame_num, camera);

        bindHdr();
        clearFramebuffer();
        frustrumCullRenderQueue(render_queue, camera);
        drawRenderQueue(render_queue, loaded_level.environment, camera);

        if (graphics::do_bloom)
            blurBloomFbo(true_dt);
        bindBackbuffer();
        drawPost(camera);

        if (Controls::editor.isAction("screenshot")) {
            auto t = std::time(nullptr);
            auto tm = *std::localtime(&t);

            std::ostringstream oss;
            oss << std::put_time(&tm, "%d-%m-%Y-%H-%M-%S");
            auto path = "data/screenshots/" + oss.str() + ".tga";
            pushInfoMessage("Screenshot " + path);
            writeFramebufferToTga(path);
        }

        // 
        // Draw guis
        //
        if (gamestate.is_active)
            drawGameGui(entities, local_assets);
        else
            drawEditorGui(entities, local_assets);

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
        entities.propogateChanges();

#ifndef NDEBUG
        gl_state.check_errors(__FILE__, __LINE__, __func__);
#endif
        frame_num++;
    } while (!Controls::editor.isAction("exit") && !glfwWindowShouldClose(window));

    return !cleanup();
}


int cleanup() {
    // cleanup physics
    cleanupPhysics();

    // Clean up SoLoud
    soloud.deinit();

#if DO_MULTITHREAD
    if (global_thread_pool)
        global_thread_pool->stop();
#endif

    // Close OpenGL window and terminate GLFW
    glfwTerminate();
    return 1;
}

bool initialize_glfw() {
    // Initialise GLFW
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW\n";
        getchar();
        return false;
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
    window = glfwCreateWindow(1024, 700, "3D Engine", NULL, NULL);
    if (window == NULL) {
        std::cerr << "Failed to open GLFW window.\n";
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);

    // Initialize GLEW
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW.\n";
        glfwTerminate();
        return false;
    }

    // Ensure we can capture the escape key being pressed below
    //glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

    glfwGetWindowSize(window, &window_width, &window_height);
    glfwSetWindowSizeCallback(window, windowSizeCallback);

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSwapInterval(0);

    return true;
}

bool initialize_sound() {
    soloud.init();
    if (soloud.getBackendString() == NULL) {
        std::cout << "Soloud failed to initialize\n";
        return false;
    }
    else {
        std::cout << "Soloud backend string: " << soloud.getBackendString() << "\n";
    }
    return true;
}

EntityManager* get_active_entities() {
    if (gamestate.is_active) {
        return &gamestate.level.entities;
    } else {
        return &loaded_level.entities;
    }
}

#include <filesystem>
#ifdef _WINDOWS
#include <windows.h>
std::string getexepath() {
    char result[MAX_PATH];
    auto p = std::filesystem::path(std::string(result, GetModuleFileName(NULL, result, MAX_PATH)));
    return p.parent_path().string();
}
#else
#include <limits.h>
#include <unistd.h>
std::string getexepath() {
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    auto p = std::filesystem::path(std::string(result, (count > 0) ? count : 0));
    return p.parent_path().string();
}
#endif
