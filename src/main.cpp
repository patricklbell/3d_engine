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
ImVec4 clear_color;

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

    initDefaultMaterial();

    Camera camera;
    createDefaultCamera(camera);

    GBuffer gbuffer;
    createGBuffer(gbuffer);

    // Load shaders
    loadGeometryShader("data/shaders/geometry.gl");
    loadPostShader("data/shaders/post.gl");
    loadPointLightShader("data/shaders/point.gl");

    // Map for last update time for hotswaping files
    std::filesystem::file_time_type empty_file_time;
    std::map<const std::string, std::pair<shader::TYPE, std::filesystem::file_time_type>> shader_update_times = {
        {"data/shaders/geometry.gl", {shader::TYPE::GEOMETRY, empty_file_time}},
        {"data/shaders/post.gl", {shader::TYPE::POST, empty_file_time}},
        {"data/shaders/point.gl", {shader::TYPE::POINT, empty_file_time}},
    };
    // Fill in with correct file time
    for (auto &pair : shader_update_times) {
        if(std::filesystem::exists(pair.first)) pair.second.second = std::filesystem::last_write_time(pair.first);
    }

    // Note screen triangle possibly better
    // Setup screen quad [-1, -1] -> [1, 1]
    //GLuint screen_quad_vao;
	//{
    //    const unsigned short indices_buffer_data[] = { 4, 3, 1, 2, 4, 1 };
    //    static const GLfloat vertex_buffer_data[] = { 
    //        -1.0f,  1.0f, 0.0f,
    //         1.0f,  1.0f, 0.0f,
    //        -1.0f, -1.0f, 0.0f,
    //         1.0f, -1.0f, 0.0f,
    //    };
    //    static const GLfloat uv_buffer_data[] = { 
    //         1.0f,  1.0f,
    //         0.0f,  1.0f,
    //         0.0f,  0.0f,
    //         1.0f,  0.0f,
    //    };
    //    GLuint vertexbuffer, indicesbuffer, uvbuffer;
    //    glGenBuffers(1, &vertexbuffer);
    //    glGenBuffers(1, &indicesbuffer);
    //    glGenBuffers(1, &uvbuffer);

    //    glBindVertexArray(screen_quad_vao);

    //    glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
    //    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), &vertex_buffer_data[0], GL_STATIC_DRAW);
    //    glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, 0);
    //    glEnableVertexAttribArray(0);

    //    glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
    //    glBufferData(GL_ARRAY_BUFFER, sizeof(uv_buffer_data), &uv_buffer_data[0], GL_STATIC_DRAW);
    //    glVertexAttribPointer(1, 2, GL_FLOAT, false, 0, 0);
    //    glEnableVertexAttribArray(1);

    //    // Generate a buffer for the indices as well
    //    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indicesbuffer);
    //    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices_buffer_data), &indices_buffer_data[0], GL_STATIC_DRAW);

    //    glBindVertexArray(0);
    //}
    Asset *quad = new Asset;
    loadAsset(quad, "data/models/quad.obj");
    quad->name = "quad";
    quad->program_id = shader::post;
    quad->draw_mode = GL_TRIANGLES;
    quad->draw_start = 0;
    quad->draw_type = GL_UNSIGNED_SHORT;

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

    sun_direction = glm::vec3(-0.7071067811865475, -0.7071067811865475, 0);
    sun_color = glm::vec3(0.941, 0.933, 0.849);

    // Load assets
    std::vector<std::string> asset_names = {"cube", "capsule", "WoodenCrate", "lamp"};
    std::vector<std::string> asset_paths = {"data/models/cube.obj", "data/models/capsule.obj", "data/models/WoodenCrate.obj", "data/models/lamp.obj"};
    std::vector<Asset *> assets;
    for (int i = 0; i < asset_paths.size(); ++i) {
        auto asset = new Asset;
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

        btTransform transform;
        transform.setFromOpenGLMatrix((float*)&entities[id]->transform[0]);

        rigidbodies[id]->setMotionState(new btDefaultMotionState(transform));

        rigidbodies[id]->setMotionState(new btDefaultMotionState(btTransform(btQuaternion(0,0,0),btVector3(0, i*3, 0))));
        dynamics_world->addRigidBody(rigidbodies[id]);
        rigidbodies[id]->setUserIndex(id);
    }

    // TODO: Skymap loading and rendering
    //GLuint tSkybox = load_cubemap({"Skybox1.bmp", "Skybox2.bmp","Skybox3.bmp","Skybox4.bmp","Skybox5.bmp","Skybox6.bmp"});
    
    loadEditorGui();
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
            createGBuffer(gbuffer);
        }

        handleEditorControls(camera, entities, true_dt);

        // Hotswap shader files
        if(current_time - last_filesystem_hotswap_check >= 1.0){
            last_filesystem_hotswap_check = current_time;
            for (auto &pair : shader_update_times) {
                if(pair.second.second != std::filesystem::last_write_time(pair.first)){
                    pair.second.second = std::filesystem::last_write_time(pair.first);
                    switch (pair.second.first) {
                        case shader::TYPE::GEOMETRY:
                            loadGeometryShader(pair.first.c_str());
                            break;
                        case shader::TYPE::POST:
                            loadPostShader(pair.first.c_str());
                            break;
                        case shader::TYPE::POINT:
                            loadPointLightShader(pair.first.c_str());
                            break;
                        default:
                            break;
                    }
                } 
            }
        }


        //dynamicsWorld->stepSimulation(true_dt);

        //// Update matrixes of physics objects
        //for (auto &rigidbody : rigidbodies) {
        //	auto object = static_cast<Entity *>(rigidbody->getUserPointer());
        //	auto pos = glm::vec3(object->transform[3]);
        //	if(object != selected_object){
        //		btTransform trans;
        //		rigidbody->getMotionState()->getWorldTransform(trans);

        //		auto vec = trans.getOrigin();
        //		//std::cout << vec.getX() << ", " << vec.getY() << ", "<< vec.getZ() << "\n";

        //		// Convert the btTransform into the GLM matrix
        //		trans.getOpenGLMatrix(glm::value_ptr(object->transform));
        //	} else {
        //		std::cout << pos.x << ", " << pos.y << ", "<< pos.z << "\n";
        //		rigidbody->setMotionState(new btDefaultMotionState(btTransform(btQuaternion(0,0,0),btVector3(pos.x, pos.y, pos.z))));
        //	}
        //}
 
        clearGBuffer(gbuffer);

        bindGbuffer(gbuffer);
        drawGeometryGbuffer(entities, camera);

        bindDeffered(gbuffer);
        drawPost(camera.position, quad);

        drawGbufferToBackbuffer(gbuffer);

        drawEditorGui(camera, entities, assets, free_entity_stack, delete_entity_stack, id_counter, rigidbodies, rigidbody_CI, gbuffer);

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
    } // Check if the ESC key was pressed or the window was closed
    while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS && glfwWindowShouldClose(window) == 0 );

    // Cleanup VAOs and shader
    for(auto &asset : assets){
        glDeleteVertexArrays(1, &asset->vao);
    }
    deleteShaderPrograms();    

    // Close OpenGL window and terminate GLFW
    glfwTerminate();

    return 0;
}


