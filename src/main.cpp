#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <vector>
#include <stack>
#include <array>
#include <map>
#include <filesystem>

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
glm::vec3 sun_color;
glm::vec3 sun_direction;

int main() {
    // Initialise GLFW
    if( !glfwInit() )
    {
        fprintf( stderr, "Failed to initialize GLFW\n" );
        getchar();
        return -1;
    }
    
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glsl_version = "";
#ifdef __APPLE__
    // GL 4.1 + GLSL 410
    glsl_version = "#version 410\n";
    glfwWindowHint( // required on Mac OS
        GLFW_OPENGL_FORWARD_COMPAT,
        GL_TRUE
    );
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
#elif __linux__
    // GL 4.3 + GLSL 430
    glsl_version = "#version 430\n";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#elif _WIN32
    // GL 3.0 + GLSL 130
    glsl_version = "#version 130\n";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
#endif

#if __APPLE__
    // to prevent 1200x800 from becoming 2400x1600
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
#endif

    // Open a window and create its OpenGL context
    window = glfwCreateWindow( 1024, 700, "Window", NULL, NULL);
    if( window == NULL ) {
        fprintf( stderr, "Failed to open GLFW window.\n" );
        getchar();
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Initialize GLEW
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW\n");
        getchar();
        glfwTerminate();
        return -1;
    }

    // Ensure we can capture the escape key being pressed below
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

    glfwSetScrollCallback(window, windowScrollCallback);
    glfwGetWindowSize(window, &window_width, &window_height);
    glfwSetWindowSizeCallback(window, windowSizeCallback);

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSwapInterval(0);

    glEnable(GL_MULTISAMPLE);

    initDefaultMaterial();
    initGraphicsPrimitives();
    createShadowFbo();
    createHdrFbo();
    if(shader::unified_bloom){
        createBloomFbo();
    }


    // @note camera instanciation uses sun direction to create shadow view
    // In future create a shadow object/directional light object which contains
    // sun_direction sun_color shadow_view ortho_projection/shadow_projection
    sun_direction = glm::vec3(-0.7071067811865475, -0.7071067811865475, 0);
    sun_color = 5.0f*glm::vec3(0.941, 0.933, 0.849);

    Camera camera;
    createDefaultCamera(camera);
    updateShadowVP(camera);

    // Load shaders
    loadNullShader("data/shaders/null.gl");
    loadUnifiedShader("data/shaders/unified.gl");
    loadWaterShader("data/shaders/water.gl");
    loadGaussianBlurShader("data/shaders/gaussian_blur.gl");
    loadPostShader("data/shaders/post.gl");
    loadDebugShader("data/shaders/debug.gl");

    // Map for last update time for hotswaping files
    std::filesystem::file_time_type empty_file_time;
    std::map<const std::string, std::pair<shader::TYPE, std::filesystem::file_time_type>> shader_update_times = {
        {"data/shaders/null.gl", {shader::TYPE::NULL_SHADER, empty_file_time}},
        {"data/shaders/unified.gl", {shader::TYPE::UNIFIED_SHADER, empty_file_time}},
        {"data/shaders/water.gl", {shader::TYPE::WATER_SHADER, empty_file_time}},
        {"data/shaders/gaussian_blur.gl", {shader::TYPE::GAUSSIAN_BLUR_SHADER, empty_file_time}},
        {"data/shaders/post.gl", {shader::TYPE::POST_SHADER, empty_file_time}},
        {"data/shaders/debug.gl", {shader::TYPE::DEBUG_SHADER, empty_file_time}},
    };
    // Fill in with correct file time
    for (auto &pair : shader_update_times) {
        if(std::filesystem::exists(pair.first)) pair.second.second = std::filesystem::last_write_time(pair.first);
    }

    // Load assets
    std::map<std::string, Asset*> assets;
    
    EntityManager entity_manager;

    loadLevel(entity_manager, assets, "data/levels/test.level");

    // @todo Skymap loading and rendering
    //GLuint tSkybox = load_cubemap({"Skybox1.bmp", "Skybox2.bmp","Skybox3.bmp","Skybox4.bmp","Skybox5.bmp","Skybox6.bmp"});
    
    initEditorGui();
    initEditorControls();

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
            createHdrFbo(true);
            if(shader::unified_bloom){
                createBloomFbo(true);
            }
        }

        // Hotswap shader files
        if(current_time - last_filesystem_hotswap_check >= 1.0){
            last_filesystem_hotswap_check = current_time;
            for (auto &pair : shader_update_times) {
                if(pair.second.second != std::filesystem::last_write_time(pair.first)){
                    pair.second.second = std::filesystem::last_write_time(pair.first);
                    switch (pair.second.first) {
                        case shader::TYPE::NULL_SHADER:
                            loadNullShader(pair.first.c_str());
                            break;
                        case shader::TYPE::UNIFIED_SHADER:
                            loadUnifiedShader(pair.first.c_str());
                            break;
                        case shader::TYPE::WATER_SHADER:
                            loadWaterShader(pair.first.c_str());
                            break;
                        case shader::TYPE::GAUSSIAN_BLUR_SHADER:
                            loadGaussianBlurShader(pair.first.c_str());
                            break;
                        case shader::TYPE::POST_SHADER:
                            loadPostShader(pair.first.c_str());
                            break;
                        case shader::TYPE::DEBUG_SHADER:
                            loadDebugShader(pair.first.c_str());
                            break;
                        default:
                            break;
                    }
                } 
            }
        }

        bindDrawShadowMap(entity_manager, camera);

        bindHdr();
        drawUnifiedHdr(entity_manager, camera);

        handleEditorControls(camera, entity_manager, true_dt);

        int blur_buffer_index = 0;
        if(shader::unified_bloom){
            blur_buffer_index = blurBloomFbo();
        }

        bindBackbuffer();
        drawPost(blur_buffer_index);

        drawEditorGui(camera, entity_manager, assets);

        // Swap backbuffer with front buffer
        glfwSwapBuffers(window);
        window_resized = false;
        glfwPollEvents();

        entity_manager.propogateChanges();
        static GLenum code;
        static const GLubyte* string;
        if(code != GL_NO_ERROR){
            code = glGetError();
            string = gluErrorString(code);
            fprintf(stderr, "<--------------------OpenGL ERROR-------------------->\n%s\n", string);
        }
    } // Check if the ESC key was pressed or the window was closed
    while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS && glfwWindowShouldClose(window) == 0 );

    // Free assets
    for(const auto &a : assets){
        delete(a.second);
    }
    for(const auto &a : editor::editor_assets){
        delete(a.second);
    }
    deleteShaderPrograms();    

    // Close OpenGL window and terminate GLFW
    glfwTerminate();

    return 0;
}


