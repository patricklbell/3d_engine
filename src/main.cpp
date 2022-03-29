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

// Include Bullet
#include <btBulletDynamicsCommon.h>

#include "globals.hpp"
#include "shader.hpp"
#include "texture.hpp"
#include "utilities.hpp"
#include "graphics.hpp"
#include "controls.hpp"
#include "editor.hpp"
#include "assets.hpp"

// Define globals.hpp
int selected_entity;
std::string glsl_version;
btDiscreteDynamicsWorld* dynamics_world;
glm::vec3 sun_color;
glm::vec3 sun_direction;
glm::vec4 clear_color = glm::vec4(67/255, 85/255, 98/255, 1.0);

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
    // GL 3.2 + GLSL 150
    glsl_version = "#version 150";
    glfwWindowHint( // required on Mac OS
        GLFW_OPENGL_FORWARD_COMPAT,
        GL_TRUE
    );
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
#elif __linux__
    // GL 3.2 + GLSL 150
    glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#elif _WIN32
    // GL 3.0 + GLSL 130
    glsl_version = "#version 130";
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

    initGraphicsPrimitives();
    initDefaultMaterial();
    createShadowFbo();
    if(shader::unified_bloom){
        createHdrFbo();
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
    loadGaussianBlurShader("data/shaders/gaussian_blur.gl");
    loadPostShader("data/shaders/post.gl");

    // Map for last update time for hotswaping files
    std::filesystem::file_time_type empty_file_time;
    std::map<const std::string, std::pair<shader::TYPE, std::filesystem::file_time_type>> shader_update_times = {
        {"data/shaders/null.gl", {shader::TYPE::NULL_SHADER, empty_file_time}},
        {"data/shaders/unified.gl", {shader::TYPE::UNIFIED_SHADER, empty_file_time}},
        {"data/shaders/gaussian_blur.gl", {shader::TYPE::GAUSSIAN_BLUR_SHADER, empty_file_time}},
        {"data/shaders/post.gl", {shader::TYPE::POST_SHADER, empty_file_time}},
    };
    // Fill in with correct file time
    for (auto &pair : shader_update_times) {
        if(std::filesystem::exists(pair.first)) pair.second.second = std::filesystem::last_write_time(pair.first);
    }

    // Create point lights
    std::vector<PointLight> point_lights = {PointLight(glm::vec3(2,0,0), glm::vec3(1,1,1), 0.5, 1, 1, 0.8)};

    // Initialize Bullet
    {
    // Build the broadphase
    btBroadphaseInterface* broadphase = new btDbvtBroadphase();

    // Set up the collision configuration and dispatcher
    btDefaultCollisionConfiguration* collision_configuration = new btDefaultCollisionConfiguration();
    btCollisionDispatcher* dispatcher = new btCollisionDispatcher(collision_configuration);

    // The actual physics solver
    btSequentialImpulseConstraintSolver* solver = new btSequentialImpulseConstraintSolver;

    // The world
    dynamics_world = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collision_configuration);
    dynamics_world->setGravity(btVector3(0, -9.81f, 0));
    }

    // @todo Load collider specific to object
    btCollisionShape* box_collision_shape = new btBoxShape(btVector3(1.0f, 1.0f, 1.0f));
    btDefaultMotionState* motionstate = new btDefaultMotionState(btTransform(btQuaternion(0,0,0), btVector3(0,0,0)));
    btRigidBody::btRigidBodyConstructionInfo rigidbody_CI(
        0.1,                    // mass, in kg. 0 -> Static object, will never move.
        motionstate,
        box_collision_shape,    // collision shape of body
        btVector3(0, 0, 0)    // local inertia
    );
    // Load assets
    std::vector<std::string> asset_names = {"cube", "sphere", "capsule", "WoodenCrate"};
    std::vector<std::string> asset_paths = {"data/models/cube.obj", "data/models/sphere.obj", "data/models/capsule.obj", "data/models/WoodenCrate.obj"};
    std::vector<Mesh *> assets;
    for (int i = 0; i < asset_paths.size(); ++i) {
        auto asset = new Mesh;
        loadAsset(asset, asset_paths[i]);
        asset->name = asset_names[i];
        assets.push_back(asset);
    }
    
    // Create instances
    btRigidBody *rigidbodies[ENTITY_COUNT] = {};
    Entity      *entities[ENTITY_COUNT]    = {};
    std::stack<int> free_entity_stack;
    std::stack<int> delete_entity_stack;
    int id_counter = 0;

    for (int i = 0; i < 1; ++i) {
        int id = id_counter++;
        entities[id] = new Entity();
        entities[id]->id = id;
        entities[id]->asset = assets[0];
        entities[id]->transform = glm::translate(glm::mat4(), glm::vec3(0, i*3, 0));
        rigidbodies[id] = new btRigidBody(rigidbody_CI);

        rigidbodies[id]->setMotionState(new btDefaultMotionState(btTransform(btQuaternion(0,0,0),btVector3(0, i*3, 0))));
        dynamics_world->addRigidBody(rigidbodies[id]);
        rigidbodies[id]->setUserIndex(id);
    }

    // TODO: Skymap loading and rendering
    //GLuint tSkybox = load_cubemap({"Skybox1.bmp", "Skybox2.bmp","Skybox3.bmp","Skybox4.bmp","Skybox5.bmp","Skybox6.bmp"});
    
    initEditorGui();
    initEditorControls();

    // Update all variables whose delta is checked
    double last_time = glfwGetTime();
    double last_filesystem_hotswap_check = last_time;
    selected_entity = -1;
    do {
        double current_time = glfwGetTime();
        float true_dt = current_time - last_time;
        last_time = current_time;
        static const float dt = 1.0/60.0;

        if (window_resized){
            updateCameraProjection(camera);
            updateShadowVP(camera);
            if(shader::unified_bloom){
                createHdrFbo();
                createBloomFbo();
            }
        }

        handleEditorControls(camera, entities, true_dt);

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
                        case shader::TYPE::GAUSSIAN_BLUR_SHADER:
                            loadGaussianBlurShader(pair.first.c_str());
                            break;
                        case shader::TYPE::POST_SHADER:
                            loadPostShader(pair.first.c_str());
                            break;
                        default:
                            break;
                    }
                } 
            }
        }


        //dynamics_world->stepSimulation(true_dt);
        // Update matrixes of physics objects
        //for (auto &rigidbody : rigidbodies) {
        //	auto entity = entities[rigidbody->getUserIndex()];
        //	auto pos = glm::vec3(entity->transform[3]);
        //	if(rigidbody->getUserIndex() != selected_entity){
        //		btTransform trans;
        //		rigidbody->getMotionState()->getWorldTransform(trans);

        //		auto vec = trans.getOrigin();
        //		//std::cout << vec.getX() << ", " << vec.getY() << ", "<< vec.getZ() << "\n";

        //		// Convert the btTransform into the GLM matrix
        //		//trans.getOpenGLMatrix(glm::value_ptr(entity->transform));
        //	} else {
        //		std::cout << pos.x << ", " << pos.y << ", "<< pos.z << "\n";
        //		rigidbody->setMotionState(new btDefaultMotionState(btTransform(btQuaternion(0,0,0),btVector3(pos.x, pos.y, pos.z))));
        //	}
        //}

        bindDrawShadowMap(entities, camera);

        bindHdr();
        drawUnifiedHdr(entities, camera);

        if(shader::unified_bloom){
            int blur_buffer_index = blurBloomFbo();
            bindBackbuffer();
            drawPost(blur_buffer_index);
        }

        drawEditorGui(camera, entities, assets, free_entity_stack, delete_entity_stack, id_counter, rigidbodies, rigidbody_CI);

        // Swap backbuffer with front buffer
        glfwSwapBuffers(window);
        glfwPollEvents();

        // Delete entities
        while(delete_entity_stack.size() != 0){
            int id = delete_entity_stack.top();
            delete_entity_stack.pop();
            free_entity_stack.push(id);
            free (entities[id]);
            entities[id] = nullptr;
            if(rigidbodies[id] != nullptr){
                dynamics_world->removeRigidBody(rigidbodies[id]);
                rigidbodies[id] = nullptr;
            }
        }

        static GLenum code;
        static const GLubyte* string;
        if(code != GL_NO_ERROR){
            code = glGetError();
            string = gluErrorString(code);
            fprintf(stderr, "<--------------------OpenGL ERROR-------------------->\n%s\n", string);
        }
    } // Check if the ESC key was pressed or the window was closed
    while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS && glfwWindowShouldClose(window) == 0 );

    // Cleanup VAOs and shader
    for(auto &asset : assets){
        glDeleteVertexArrays(1, &asset->vao);
        for(int i = 0; i<asset->num_materials; ++i){
    	    glDeleteTextures(1, &asset->materials[i]->t_albedo);
    	    glDeleteTextures(1, &asset->materials[i]->t_normal);
    	    glDeleteTextures(1, &asset->materials[i]->t_metallic);
    	    glDeleteTextures(1, &asset->materials[i]->t_roughness);
        }
    }
    deleteShaderPrograms();    

    // Close OpenGL window and terminate GLFW
    glfwTerminate();

    return 0;
}


