// Include standard headers
#include "graphics.hpp"
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
#include <vector>
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

#include "shader.hpp"
#include "texture.hpp"
#include "utilities.hpp"
#include "vboindexer.hpp"
#include "objloader.hpp"

// Include Bullet
#include <btBulletDynamicsCommon.h>

#define ENTITY_COUNT 1000

double globalScrollXOffset = 0;
double globalScrollYOffset = 0;

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset){
    globalScrollXOffset = xoffset;
    globalScrollYOffset = yoffset;
}


/*! \enum CameraState
 *
 */
int main( void )
{
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

    std::string glsl_version = "";
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

    glfwSetScrollCallback(window, scroll_callback);

    // Enable depth test
    glEnable(GL_DEPTH_TEST);
    // Accept fragment if it closer to the camera than the former one
    glDepthFunc(GL_LESS);

    // Projection matrix : 45 Field of View, screen ratio, display range : 0.1 unit <-> 100 units
    // Scale to window size
    glm::mat4 Projection;
    GBuffer gbuffer;
    {
        GLint window_width, window_height;
	    glfwGetWindowSize(window, &window_width, &window_height);
        gbuffer = generate_gbuffer(window_width, window_height);
        Projection = glm::perspective(glm::radians(45.0f), (float)window_width/(float)window_height, 0.1f, 100.0f);
    }
	
    // Create and compile our GLSL program from the shaders
    GLuint geometryProgramID = load_shader("data/shaders/geometry.gl","",true);

    // grab geom uniforms to modify
    GLuint u_geom_MVP = glGetUniformLocation(geometryProgramID, "MVP");
    GLuint u_geom_model = glGetUniformLocation(geometryProgramID, "model");

    glUseProgram(geometryProgramID);
    glUniform1i(glGetUniformLocation(geometryProgramID, "diffuseMap"), 0);
    glUniform1i(glGetUniformLocation(geometryProgramID, "normalMap"),  1);

    // Deferred shaders
    GLuint directionalProgramID = load_shader("data/shaders/light_pass.vert", "data/shaders/dir_light_pass.frag",false);
    GLuint u_dir_screen_size = glGetUniformLocation(directionalProgramID, "screenSize");
    GLuint u_dir_light_color = glGetUniformLocation(directionalProgramID, "lightColor");
    GLuint u_dir_light_direction = glGetUniformLocation(directionalProgramID, "lightDirection");
    GLuint u_dir_camera_position = glGetUniformLocation(directionalProgramID, "cameraPosition");

    glUseProgram(directionalProgramID);
    const glm::mat4 identity = glm::mat4();
    glUniformMatrix4fv(glGetUniformLocation(directionalProgramID, "MVP"), 1, GL_FALSE, &identity[0][0]);
    glUniform1i(glGetUniformLocation(directionalProgramID, "positionMap"), GBuffer::GBUFFER_TEXTURE_TYPE_POSITION);
    glUniform1i(glGetUniformLocation(directionalProgramID, "normalMap"), GBuffer::GBUFFER_TEXTURE_TYPE_NORMAL);
    glUniform1i(glGetUniformLocation(directionalProgramID, "diffuseMap"), GBuffer::GBUFFER_TEXTURE_TYPE_DIFFUSE);

    GLuint pointProgramID = load_shader("data/shaders/light_pass.vert", "data/shaders/point_light_pass.frag",false);

    glUseProgram(pointProgramID);
    glUniformMatrix4fv(glGetUniformLocation(pointProgramID, "MVP"), 1, GL_FALSE, &identity[0][0]);
    glUniform1i(glGetUniformLocation(pointProgramID, "positionMap"), GBuffer::GBUFFER_TEXTURE_TYPE_POSITION);
    glUniform1i(glGetUniformLocation(pointProgramID, "normalMap"), GBuffer::GBUFFER_TEXTURE_TYPE_NORMAL);
    glUniform1i(glGetUniformLocation(pointProgramID, "diffuseMap"), GBuffer::GBUFFER_TEXTURE_TYPE_DIFFUSE);

    std::filesystem::file_time_type empty_file_time;
    std::map<std::string, std::map<std::string, std::filesystem::file_time_type>> shaderUpdateTimes = {
        {"geometryProgramID", {{"data/shaders/geometry.gl", empty_file_time}}},
        {"directionalProgramID", {{"data/shaders/light_pass.vert", empty_file_time}, {"data/shaders/dir_light_pass.frag",empty_file_time}}},
        {"pointProgramID", {{"data/shaders/light_pass.vert", empty_file_time}, {"data/shaders/dir_light_pass.frag", empty_file_time}}}
    };
    for (auto &program : shaderUpdateTimes) {
       for (auto &pair : program.second) {
           pair.second = std::filesystem::last_write_time(pair.first);
       } 
    }

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
    ModelAsset *quad = new ModelAsset;
    load_asset(quad, "data/models/quad.obj", "");
    quad->name = "quad";
    quad->programID = directionalProgramID;
    quad->drawMode = GL_TRIANGLES;
    quad->drawStart = 0;
    quad->drawType = GL_UNSIGNED_SHORT;

    // Initialize Bullet
    // Build the broadphase
    btBroadphaseInterface* broadphase = new btDbvtBroadphase();

    // Set up the collision configuration and dispatcher
    btDefaultCollisionConfiguration* collisionConfiguration = new btDefaultCollisionConfiguration();
    btCollisionDispatcher* dispatcher = new btCollisionDispatcher(collisionConfiguration);

    // The actual physics solver
    btSequentialImpulseConstraintSolver* solver = new btSequentialImpulseConstraintSolver;

    // The world
    btDiscreteDynamicsWorld* dynamicsWorld = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collisionConfiguration);
    dynamicsWorld->setGravity(btVector3(0, -9.81f, 0));

    btCollisionShape* boxCollisionShape = new btBoxShape(btVector3(1.0f, 1.0f, 1.0f));
    btDefaultMotionState* motionstate = new btDefaultMotionState(btTransform(btQuaternion(0,0,0), btVector3(0,0,0)));

    btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(
        0.1,                    // mass, in kg. 0 -> Static object, will never move.
        motionstate,
        boxCollisionShape,    // collision shape of body
        btVector3(0, 0, 0)    // local inertia
    );

    glm::vec3 sun_direction = glm::vec3(-0.7071067811865475, -0.7071067811865475, 0);
    glm::vec3 sun_color = glm::vec3(0.941, 0.933, 0.849);

	// Setup bullet debug renderer
	//GLDebugDrawer btDebugDrawer;
    //dynamicsWorld->setDebugDrawer(&btDebugDrawer);
	//btDebugDrawer.setDebugMode(1);

    // Load assets
    std::vector<std::string> asset_names = {"cube", "capsule", "WoodenCrate"};
    std::vector<ModelAsset *> assets;
    for (auto path : asset_names) {
        auto asset = new ModelAsset;
        asset->mat = new Material;
        load_asset(asset, "data/models/"+path+".obj", "data/models/" + path+".mtl");
        asset->name = path;
        asset->programID = geometryProgramID;
        asset->drawMode = GL_TRIANGLES;
        asset->drawStart = 0;
        asset->drawType = GL_UNSIGNED_SHORT;

        assets.push_back(asset);
    }
    GLuint default_tDiffuse = load_image("data/textures/default_diffuse.bmp");
    GLuint default_tNormal  = load_image("data/textures/default_normal.bmp");
    
    // Create instances
    btRigidBody *rigidbodies[ENTITY_COUNT] = {};
    Entity      *entities[ENTITY_COUNT]    = {};
    std::stack<int> freeIdStack;
    std::stack<int> deleteIdStack;
    int idCounter = 0;

    for (int i = 0; i < 1; ++i) {
        int id = idCounter++;
        entities[id] = new Entity();
        entities[id]->id = id;
        entities[id]->asset = assets[0];
        entities[id]->transform = glm::translate(glm::mat4(), glm::vec3(0, i*3, 0));
        rigidbodies[id] = new btRigidBody(rigidBodyCI);

        btTransform transform;
        transform.setFromOpenGLMatrix((float*)&entities[id]->transform[0]);

        rigidbodies[id]->setMotionState(new btDefaultMotionState(transform));

        rigidbodies[id]->setMotionState(new btDefaultMotionState(btTransform(btQuaternion(0,0,0),btVector3(0, i*3, 0))));
        dynamicsWorld->addRigidBody(rigidbodies[id]);
        rigidbodies[id]->setUserIndex(id);
    }

    //GLuint tSkybox = load_cubemap({"Skybox1.bmp", "Skybox2.bmp","Skybox3.bmp","Skybox4.bmp","Skybox5.bmp","Skybox6.bmp"});

    // Update all variables whose delta is checked
    double old_xpos, old_ypos;
    int selected_entity = -1;
    double last_time = glfwGetTime();
    double last_filesystem_hotswap_time = last_time;
    glfwGetWindowSize(window, &window_width, &window_height);
    glfwSetWindowSizeCallback(window, window_resize_callback);
    do {
        if (window_resized){
            update_camera_projection(camera);
            gbuffer = generate_gbuffer();
        }

        double current_time = glfwGetTime();
        float true_dt = current_time - last_time;
        last_time = current_time;
        float dt = 1.0/60.0;

        // Hotswap shader files
        if(current_time - last_filesystem_hotswap_time >= 1.0){
            last_filesystem_hotswap_time = current_time;
            for (auto &program : shaderUpdateTimes) {
                for (auto &pair : program.second) {
                    if(pair.second != std::filesystem::last_write_time(pair.first)){
                        std::cout << "Reloading shaders.\n"; 
                        pair.second = std::filesystem::last_write_time(pair.first);
                        if(program.first == "geometryProgramID"){
                            // Create and compile our GLSL program from the shaders
                            geometryProgramID = load_shader("data/shaders/geometry.gl","",true);

                            // grab geom uniforms to modify
                            GLuint u_geom_MVP = glGetUniformLocation(geometryProgramID, "MVP");
                            GLuint u_geom_model = glGetUniformLocation(geometryProgramID, "model");

                            glUseProgram(geometryProgramID);
                            glUniform1i(glGetUniformLocation(geometryProgramID, "diffuseMap"), 0);
                            glUniform1i(glGetUniformLocation(geometryProgramID, "normalMap"),  1);
                        }
                        else if(program.first == "directionalProgramID"){
                            // Deferred shaders
                            directionalProgramID = load_shader("data/shaders/light_pass.vert", "data/shaders/dir_light_pass.frag",false);
                            GLuint u_dir_screen_size = glGetUniformLocation(directionalProgramID, "screenSize");
                            GLuint u_dir_light_color = glGetUniformLocation(directionalProgramID, "lightColor");
                            GLuint u_dir_light_direction = glGetUniformLocation(directionalProgramID, "lightDirection");
                            GLuint u_dir_camera_position = glGetUniformLocation(directionalProgramID, "cameraPosition");

                            glUseProgram(directionalProgramID);
                            const glm::mat4 identity = glm::mat4();
                            glUniformMatrix4fv(glGetUniformLocation(directionalProgramID, "MVP"), 1, GL_FALSE, &identity[0][0]);
                            glUniform1i(glGetUniformLocation(directionalProgramID, "positionMap"), GBuffer::GBUFFER_TEXTURE_TYPE_POSITION);
                            glUniform1i(glGetUniformLocation(directionalProgramID, "normalMap"), GBuffer::GBUFFER_TEXTURE_TYPE_NORMAL);
                            glUniform1i(glGetUniformLocation(directionalProgramID, "diffuseMap"), GBuffer::GBUFFER_TEXTURE_TYPE_DIFFUSE);
                        }
                        else if(program.first == "pointProgramID"){
                            pointProgramID = load_shader("data/shaders/light_pass.vert", "data/shaders/point_light_pass.frag",false);

                            glUseProgram(pointProgramID);
                            glUniformMatrix4fv(glGetUniformLocation(pointProgramID, "MVP"), 1, GL_FALSE, &identity[0][0]);
                            glUniform1i(glGetUniformLocation(pointProgramID, "positionMap"), GBuffer::GBUFFER_TEXTURE_TYPE_POSITION);
                            glUniform1i(glGetUniformLocation(pointProgramID, "normalMap"), GBuffer::GBUFFER_TEXTURE_TYPE_NORMAL);
                            glUniform1i(glGetUniformLocation(pointProgramID, "diffuseMap"), GBuffer::GBUFFER_TEXTURE_TYPE_DIFFUSE);
                        }
                        break;
                    }
                } 
            }
        }

        // Process Events
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        static int oldKeySpaceState = GLFW_RELEASE;
        if(glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && oldKeySpaceState == GLFW_RELEASE){
            if(cameraState == TRACKBALL) {
                cameraState = SHOOTER;

                // Reset mouse position for next frame
                xpos = (float)window_width/2;
                ypos = (float)window_height/2;

                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                selected_entity = -1;
            } else if(cameraState == SHOOTER) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                cameraState = TRACKBALL;

                View = glm::lookAt(cameraPosition, cameraPivot, cameraUp);
            }
        }
        oldKeySpaceState = glfwGetKey(window, GLFW_KEY_SPACE);
        
        if(cameraState == TRACKBALL){
            static int oldMouseLeftState = GLFW_RELEASE;
            static double mouseLeftPressTime = glfwGetTime();
            int mouseLeftState = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
            if (!io.WantCaptureMouse && cameraState == TRACKBALL && mouseLeftState == GLFW_RELEASE && oldMouseLeftState == GLFW_PRESS && (glfwGetTime() - mouseLeftPressTime) < 0.2) {
                glm::vec3 out_origin;
                glm::vec3 out_direction;
                screen_pos_to_world_ray(
                    xpos, ypos,
                    window_width, window_height,
                    View,
                    Projection,
                    out_origin,
                    out_direction
                );

                glm::vec3 out_end = out_origin + out_direction * 1000.0f;
                glm::vec3 out_end_test = out_origin + out_direction * 2.0f;

                btCollisionWorld::ClosestRayResultCallback RayCallback(btVector3(out_origin.x, out_origin.y, out_origin.z), btVector3(out_end.x, out_end.y, out_end.z));
                dynamicsWorld->rayTest(btVector3(out_origin.x, out_origin.y, out_origin.z), btVector3(out_end.x, out_end.y, out_end.z), RayCallback);

                if (RayCallback.hasHit()) {
                    selected_entity = RayCallback.m_collisionObject->getUserIndex();
                    if (entities[selected_entity] == nullptr)
                        selected_entity = -1;
                    std::cout << "Selected game object " << selected_entity << "\n";
                    // Maintain relative position of old selected entity
                    //cameraPosition = glm::vec3(entities[selected_entity]->transform[3]) + cameraPosition - cameraPivot;
                    cameraPivot = glm::vec3(entities[selected_entity]->transform[3]);
                    View = glm::lookAt(cameraPosition, cameraPivot, cameraUp);
                } else {
                    selected_entity = -1;
                }
            } else if (!io.WantCaptureMouse && mouseLeftState == GLFW_PRESS) {
                if(oldMouseLeftState == GLFW_RELEASE){
                    mouseLeftPressTime = glfwGetTime();
                }
                auto cameraRight = glm::vec3(glm::transpose(View)[0]);
                cameraPosition = cameraPivot + glm::normalize(cameraPosition - cameraPivot)*cameraDistance;

                // step 1 : Calculate the amount of rotation given the mouse movement.
                float deltaAngleX = (2 * M_PI / (float)window_width); // a movement from left to right = 2*PI = 360 deg
                float deltaAngleY = (M_PI / (float)window_height);  // a movement from top to bottom = PI = 180 deg
                float xAngle = (old_xpos - xpos) * deltaAngleX;
                float yAngle = (old_ypos - ypos) * deltaAngleY;

                // Extra step to handle the problem when the camera direction is the same as the up vector
                //auto cameraViewDir = glm::vec3(-glm::transpose(View)[2]);
                //float cosAngle = glm::dot(cameraViewDir, cameraUp);
                //if (cosAngle * sgn(yAngle) > 0.99f)
                //    yAngle = 0;

                // step 2: Rotate the camera around the pivot point on the first axis.
                auto rotationMatrixX = glm::rotate(glm::mat4x4(1.0f), xAngle, cameraUp);
                auto cameraPositionRotated = glm::vec3(rotationMatrixX * glm::vec4(cameraPosition - cameraPivot, 1)) + cameraPivot;

                // step 3: Rotate the camera around the pivot point on the second axis.
                auto rotationMatrixY = glm::rotate(glm::mat4x4(1.0f), yAngle, cameraRight);
                cameraPositionRotated = glm::vec3(rotationMatrixY * glm::vec4(cameraPositionRotated - cameraPivot, 1)) + cameraPivot;

                // Update the camera view
                View = glm::lookAt(cameraPositionRotated, cameraPivot, cameraUp);

                cameraPosition = cameraPositionRotated;
            }
            if(!io.WantCaptureMouse && globalScrollYOffset != 0){
                cameraDistance = abs(cameraDistance + cameraDistance*globalScrollYOffset*0.1);

                // Handle scroll event
                globalScrollYOffset = 0;

                cameraPosition = cameraPivot + glm::normalize(cameraPosition - cameraPivot)*cameraDistance;

                // Update the camera view (we keep the same lookat and the same up vector)
                View = glm::lookAt(cameraPosition, cameraPivot, cameraUp);
            }
            oldMouseLeftState = mouseLeftState;
        } else if (cameraState == SHOOTER){
            static const float cameraMovementSpeed = 2.0;

            // Reset mouse position for next frame
            glfwSetCursorPos(window, (float)window_width/2, (float)window_height/2);
            
            glm::vec3 cameraDirection = glm::normalize(cameraPivot - cameraPosition);

            // step 1 : Calculate the amount of rotation given the mouse movement.
            float deltaAngleX = (2 * M_PI / (float)window_width); // a movement from left to right = 2*PI = 360 deg
            float deltaAngleY = (M_PI / (float)window_height);  // a movement from top to bottom = PI = 180 deg
                                                               //
            float xAngle = (float)((float)window_width/2 -  xpos) * deltaAngleX;
            float yAngle = (float)((float)window_height/2 - ypos) * deltaAngleY;

            // Extra step to handle the problem when the camera direction is the same as the up vector
            //auto cameraViewDir = glm::vec3(-glm::transpose(View)[2]);
            //float cosAngle = glm::dot(cameraViewDir, cameraUp);
            //if (cosAngle * sgn(yAngle) > 0.99f)
            //    yAngle = 0;

            // step 2: Rotate the camera around the pivot point on the first axis.
            auto rotationMatrixX = glm::rotate(glm::mat4x4(1.0f), xAngle, cameraUp);
            auto cameraRight = glm::vec3(glm::transpose(View)[0]);
            auto rotationMatrixY = glm::rotate(glm::mat4x4(1.0f), yAngle, cameraRight);

            glm::vec3 cameraDirectionRotated = glm::vec3(rotationMatrixY * (rotationMatrixX * glm::vec4(cameraDirection, 1)));

            // Move forward
            if (glfwGetKey( window, GLFW_KEY_UP ) == GLFW_PRESS){
                cameraPosition += cameraDirection * true_dt * cameraMovementSpeed;
            }
            // Move backward
            if (glfwGetKey( window, GLFW_KEY_DOWN ) == GLFW_PRESS){
                cameraPosition -= cameraDirection * true_dt * cameraMovementSpeed;
            }
            // Strafe right
            if (glfwGetKey( window, GLFW_KEY_RIGHT ) == GLFW_PRESS){
                cameraPosition += cameraRight * true_dt * cameraMovementSpeed;
            }
            // Strafe left
            if (glfwGetKey( window, GLFW_KEY_LEFT ) == GLFW_PRESS){
                cameraPosition -= cameraRight * true_dt * cameraMovementSpeed;
            }

            View = glm::lookAt(cameraPosition, cameraPosition + cameraDirectionRotated, cameraUp);
            cameraPivot = cameraPosition + cameraDirectionRotated;
        }

        // Update the mouse position for the next rotation
        old_xpos = xpos;
        old_ypos = ypos;

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
 
        // Clear attachment 4 from previous frame

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gbuffer.fbo);
        glDrawBuffer(GL_COLOR_ATTACHMENT4);
        glClearColor(0,0,0,0);
        glClear(GL_COLOR_BUFFER_BIT);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gbuffer.fbo);
        static const GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
        glDrawBuffers(3, drawBuffers);

        // Only the geometry pass updates the depth buffer
        glDepthMask(GL_TRUE);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        // Note face culling wont work with certain techniques ie grass
        glEnable(GL_CULL_FACE);

        for (auto entity : entities) {
            if(entity == nullptr)
                continue;

            ModelAsset* asset = entity->asset;
            glUseProgram(asset->programID);

            auto MVP = Projection * View * entity->transform;

            glUniformMatrix4fv(u_geom_MVP, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(u_geom_model, 1, GL_FALSE, &entity->transform[0][0]);
            
            glActiveTexture(GL_TEXTURE0);
            if(asset->mat->tDiffuse != GL_FALSE){
                glBindTexture(GL_TEXTURE_2D, asset->mat->tDiffuse);
            } else {
                glBindTexture(GL_TEXTURE_2D, default_tDiffuse);
            }

            glActiveTexture(GL_TEXTURE1);
            if(asset->mat->tNormal != GL_FALSE){
                glBindTexture(GL_TEXTURE_2D, asset->mat->tNormal);
            } else {
                glBindTexture(GL_TEXTURE_2D, default_tNormal);
            }

            //bind VAO and draw
            glBindVertexArray(asset->vao);
            glDrawElements(asset->drawMode, asset->drawCount, asset->drawType, (void*)asset->drawStart);
        }
        glBindVertexArray(0);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);

        // Point light calculations
        glEnable(GL_STENCIL_TEST);
        glDisable(GL_STENCIL_TEST);

        // Draw to attachment 4 during light pass and bind framebuffer textures
        glDrawBuffer(GL_COLOR_ATTACHMENT4);
        for (unsigned int i = 0 ; i < GBuffer::GBUFFER_NUM_TEXTURES; i++) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, gbuffer.textures[GBuffer::GBUFFER_TEXTURE_TYPE_POSITION + i]);
        }

        // Directional light pass
        glUseProgram(directionalProgramID);
        
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_ONE, GL_ONE);

        glUniform2f(u_dir_screen_size, window_width, window_height);
        glUniform3fv(u_dir_light_color, 1, &sun_color[0]);
        glUniform3fv(u_dir_light_direction, 1, &sun_direction[0]);
        glUniform3fv(u_dir_camera_position, 1, &cameraPosition[0]);

        //glBindVertexArray(screen_quad_vao);
        //glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
        glBindVertexArray(quad->vao);
        glDrawElements(quad->drawMode, quad->drawCount, quad->drawType, (void*)quad->drawStart);
        glBindVertexArray(0);

        glDisable(GL_BLEND);

        // Copy color buffer to framebuffer and draw to screen
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, gbuffer.fbo);
        glReadBuffer(GL_COLOR_ATTACHMENT4);

        glBlitFramebuffer(0, 0, window_width, window_height, 0, 0, window_width, window_height, GL_COLOR_BUFFER_BIT, GL_LINEAR);

        // Draw bt debug
        //btDebugDrawer.setMVP(Projection * View);
        //dynamicsWorld->debugDrawWorld();

        // Start the Dear ImGui frame;
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuizmo::SetOrthographic(true);
        ImGuizmo::BeginFrame();
        {
            if(selected_entity != -1) {
                ImGui::Begin("Model Properties");

                edit_transform((float*)&View[0], (float*)&Projection[0], (float*)&entities[selected_entity]->transform[0], cameraDistance);
                dynamicsWorld->removeRigidBody(rigidbodies[selected_entity]);
                
                if(cameraState == TRACKBALL){
                    cameraPivot = glm::vec3(entities[selected_entity]->transform[3]);
                }

                btTransform transform;
                transform.setFromOpenGLMatrix((float*)&entities[selected_entity]->transform[0]);

        		rigidbodies[selected_entity]->setMotionState(new btDefaultMotionState(transform));
                dynamicsWorld->addRigidBody(rigidbodies[selected_entity]);
                
                if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::ColorEdit3("Albedo Color", entities[selected_entity]->asset->mat->albedo);
                    ImGui::ColorEdit3("Diffuse Color", entities[selected_entity]->asset->mat->diffuse);
                    ImGui::InputFloat("Specular Color", &entities[selected_entity]->asset->mat->opticDensity);
                    ImGui::InputFloat("Reflection Sharpness", &entities[selected_entity]->asset->mat->reflectSharp);
                    ImGui::InputFloat("Specular Exponent (size)", &entities[selected_entity]->asset->mat->specExp);
                    ImGui::InputFloat("Dissolve", &entities[selected_entity]->asset->mat->dissolve);
                    ImGui::ColorEdit3("Transmission Filter", entities[selected_entity]->asset->mat->transFilter);
                    
                    void * texDiffuse = (entities[selected_entity]->asset->mat->tDiffuse != GL_FALSE) ? (void *)(intptr_t)entities[selected_entity]->asset->mat->tDiffuse : (void *)(intptr_t)default_tDiffuse;
                    if(ImGui::ImageButton(texDiffuse, ImVec2(128,128))){
                        fileDialogType = "asset.mat.tDiffuse";
                        fileDialog.SetTypeFilters({ ".bmp" });
                        fileDialog.Open();
                    }

                    ImGui::SameLine();

                    void * texNormal = (entities[selected_entity]->asset->mat->tNormal != GL_FALSE) ? (void *)(intptr_t)entities[selected_entity]->asset->mat->tNormal : (void *)(intptr_t)default_tNormal;
                    if(ImGui::ImageButton(texNormal, ImVec2(128,128))){
                        fileDialogType = "asset.mat.tNormal";
                        fileDialog.SetTypeFilters({ ".bmp" });
                        fileDialog.Open();
                    }
                }
                if(ImGui::Button("Duplicate")){
                    int id;
                    if(freeIdStack.size() == 0){
                        id = idCounter++;
                    } else {
                        id = freeIdStack.top();
                        freeIdStack.pop();
                    }
                    entities[id] = new Entity();
                    entities[id]->id = id;
                    entities[id]->asset = entities[selected_entity]->asset;
                    entities[id]->transform = entities[selected_entity]->transform * glm::translate(glm::mat4(), glm::vec3(0.1));
                    if(rigidbodies[selected_entity] != nullptr){
                        rigidbodies[id] = new btRigidBody(
                            1/rigidbodies[selected_entity]->getInvMass(),
                            rigidbodies[selected_entity]->getMotionState(),
                            rigidbodies[selected_entity]->getCollisionShape(),
                            btVector3(0,0,0)
                        );
                        rigidbodies[id]->translate(btVector3(0.1, 0.1, 0.1));
                        dynamicsWorld->addRigidBody(rigidbodies[id]);
                        rigidbodies[id]->setUserIndex(id);
                    }
                    if(cameraState == TRACKBALL){
                        selected_entity = id;
                        cameraPivot = glm::vec3(entities[selected_entity]->transform[3]);
                        View = glm::lookAt(cameraPosition, cameraPivot, cameraUp);
                    }
                }
                ImGui::SameLine();
                if(ImGui::Button("Delete")){
                    deleteIdStack.push(selected_entity);
                    selected_entity = -1;
                }
                ImGui::End();
            }
            //float viewManipulateRight = io.DisplaySize.x;
            //float viewManipulateTop = 0;

            //ImGuizmo::ViewManipulate((float*)&View[0], cameraDistance, ImVec2(viewManipulateRight - 128, viewManipulateTop), ImVec2(128, 128), 0x10101010);
        }
        {
            ImGui::Begin("Global Properties");
            if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (ImGui::SliderFloat("Distance", &cameraDistance, 1.f, 100.f)) {
                    cameraPosition = cameraPivot + glm::normalize(cameraPosition - cameraPivot)*cameraDistance;

                    // Update the camera view (we keep the same lookat and the same up vector)
                    View = glm::lookAt(cameraPosition, cameraPivot, cameraUp);
                }
            }
            if (ImGui::CollapsingHeader("Add Entity", ImGuiTreeNodeFlags_DefaultOpen)){
                static int asset_current = 0;

                if (ImGui::BeginCombo("##asset-combo", assets[asset_current]->name.c_str())){
                    for (int n = 0; n < assets.size(); n++)
                    {
                        bool is_selected = (asset_current == n); // You can store your selection however you want, outside or inside your objects
                        if (ImGui::Selectable(assets[n]->name.c_str(), is_selected))
                            asset_current = n;
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();   // You may set the initial focus when opening the combo (scrolling + for keyboard navigation support)
                    }
                    ImGui::EndCombo();
                }

                if(ImGui::Button("Add Instance")){
                    int id;
                    if(freeIdStack.size() == 0){
                        id = idCounter++;
                    } else {
                        id = freeIdStack.top();
                        freeIdStack.pop();
                    }
                    entities[id] = new Entity();
                    entities[id]->id = id;
                    entities[id]->asset = assets[asset_current];
                    entities[id]->transform = glm::mat4();

                    // TODO: Integrate collider with asset loading
                    rigidbodies[id] = new btRigidBody(rigidBodyCI);
                    rigidbodies[id]->setMotionState(new btDefaultMotionState(btTransform(btQuaternion(0,0,0),btVector3(0, 0, 0))));
                    dynamicsWorld->addRigidBody(rigidbodies[id]);
                    rigidbodies[id]->setUserIndex(id);
                    if(cameraState == TRACKBALL){
                        selected_entity = id;
                        cameraPivot = glm::vec3(entities[selected_entity]->transform[3]);
                        View = glm::lookAt(cameraPosition, cameraPivot, cameraUp);
                    }
                }
            }
            
            ImGui::ColorEdit3("Clear color", (float*)&clear_color); // Edit 3 floats representing a color
            ImGui::ColorEdit3("Sun color", (float*)&sun_color); // Edit 3 floats representing a color
            if(ImGui::InputFloat3("Sun direction", (float*)&sun_direction))
                sun_direction = glm::normalize(sun_direction);
            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu("Menu"))
                {
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();

            ImGui::Begin("GBuffer");
            {
                ImGui::Image((void *)(intptr_t)gbuffer.textures[GBuffer::GBUFFER_TEXTURE_TYPE_DIFFUSE], ImVec2((int)window_width/10, (int)window_height/10), ImVec2(0, 1), ImVec2(1, 0));
                ImGui::SameLine();
                ImGui::Image((void *)(intptr_t)gbuffer.textures[GBuffer::GBUFFER_TEXTURE_TYPE_NORMAL], ImVec2((int)window_width/10, (int)window_height/10), ImVec2(0, 1), ImVec2(1, 0));
                ImGui::SameLine();
                ImGui::Image((void *)(intptr_t)gbuffer.textures[GBuffer::GBUFFER_TEXTURE_TYPE_POSITION], ImVec2((int)window_width/10, (int)window_height/10), ImVec2(0, 1), ImVec2(1, 0)) ;
            }
            ImGui::End();
            fileDialog.Display();
            if(fileDialog.HasSelected())
            {
                std::cout << "Selected filename: " << fileDialog.GetSelected().string() << std::endl;
                if(fileDialogType == "asset.mat.tDiffuse"){
                    if(selected_entity != -1)
                        entities[selected_entity]->asset->mat->tDiffuse = load_image(fileDialog.GetSelected().string());
                }
                else if(fileDialogType == "asset.mat.tNormal"){
                    if(selected_entity != -1)
                        entities[selected_entity]->asset->mat->tNormal = load_image(fileDialog.GetSelected().string());
                }
                fileDialog.ClearSelected();
            }
        }

        // Rendering ImGUI
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swap buffers
        glfwSwapBuffers(window);
        glfwPollEvents();

        // Delete entities
        while(deleteIdStack.size() != 0){
            int id = deleteIdStack.top();
            deleteIdStack.pop();
            freeIdStack.push(id);
            free (entities[id]);
            entities[id] = nullptr;
            if(rigidbodies[id] != nullptr){
                dynamicsWorld->removeRigidBody(rigidbodies[id]);
                rigidbodies[id] = nullptr;
            }
        }
    } // Check if the ESC key was pressed or the window was closed
    while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS &&
            glfwWindowShouldClose(window) == 0 );
    // Cleanup VAOs and shader
    for(auto &asset : assets){
        glDeleteVertexArrays(1, &asset->vao);
        glDeleteProgram(asset->programID);
    }
        
    // Close OpenGL window and terminate GLFW
    glfwTerminate();

    return 0;
}


