// Include standard headers
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <vector>
#include <stack>
#include <array>

// Include ImGui
#include "imgui.h"
#include "ImGuizmo.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imfilebrowser.hpp"

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
enum CameraState { 
    TRACKBALL = 0,
    SHOOTER = 1,
};

bool edit_transform(float* cameraView, float* cameraProjection, float* matrix, float cameraDistance)
{
    static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
    static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::LOCAL);
    static bool useSnap = false;
    static float snap[3] = { 1.f, 1.f, 1.f };
    static float bounds[] = { -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };
    static float boundsSnap[] = { 0.1f, 0.1f, 0.1f };
    static bool boundSizing = false;
    static bool boundSizingSnap = false;
    bool change_occured = false;

    if (ImGui::IsKeyPressed(90))
        mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(69))
        mCurrentGizmoOperation = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed(82)) // r Key
        mCurrentGizmoOperation = ImGuizmo::SCALE;
    if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
        mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE))
        mCurrentGizmoOperation = ImGuizmo::ROTATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
        mCurrentGizmoOperation = ImGuizmo::SCALE;
    float matrixTranslation[3], matrixRotation[3], matrixScale[3];
    ImGuizmo::DecomposeMatrixToComponents(matrix, matrixTranslation, matrixRotation, matrixScale);
    if(ImGui::InputFloat3("Tr", matrixTranslation)) change_occured = true;
    if(ImGui::InputFloat3("Rt", matrixRotation)) change_occured = true;
    if(ImGui::InputFloat3("Sc", matrixScale)) change_occured = true;
    ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, matrix);

    if (ImGui::IsKeyPressed(83))
        useSnap = !useSnap;
    ImGui::Checkbox("", &useSnap);
    ImGui::SameLine();

    switch (mCurrentGizmoOperation)
    {
    case ImGuizmo::TRANSLATE:
        ImGui::InputFloat3("Snap", &snap[0]);
        break;
    case ImGuizmo::ROTATE:
        ImGui::InputFloat("Angle Snap", &snap[0]);
        break;
    case ImGuizmo::SCALE:
        ImGui::InputFloat("Scale Snap", &snap[0]);
        break;
    }
    ImGui::Checkbox("Bound Sizing", &boundSizing);
    if (boundSizing)
    {
        ImGui::PushID(3);
        ImGui::Checkbox("", &boundSizingSnap);
        ImGui::SameLine();
        ImGui::InputFloat3("Snap", boundsSnap);
        ImGui::PopID();
    }

    ImGuiIO& io = ImGui::GetIO();
    float viewManipulateRight = io.DisplaySize.x;
    float viewManipulateTop = 0;
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

    ImGuizmo::Manipulate(cameraView, cameraProjection, mCurrentGizmoOperation, mCurrentGizmoMode, matrix, NULL, useSnap ? &snap[0] : NULL, boundSizing ? bounds : NULL, boundSizingSnap ? boundsSnap : NULL);
    return change_occured || ImGuizmo::IsUsing();
}

class GLDebugDrawer : public btIDebugDraw {
    GLuint shaderProgram;
    glm::mat4 MVP;
    GLuint VBO, VAO;
    int m_debugMode;

public:
    GLDebugDrawer()
    {
        MVP = glm::mat4(1.0f);
        std::cout<<"Intialising debug drawer\n"; 

        const char *vertexShaderSource = "#version 330 core\n"
                                         "layout (location = 0) in vec3 aPos;\n"
                                         "uniform mat4 MVP;\n"
                                         "void main()\n"
                                         "{\n"
                                         "   gl_Position = MVP * vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
                                         "}\0";
        const char *fragmentShaderSource = "#version 330 core\n"
                                           "out vec4 FragColor;\n"
                                           "uniform vec3 color;\n"
                                           "void main()\n"
                                           "{\n"
                                           "   FragColor = vec4(color, 1.0f);\n"
                                           "}\n\0";

        // vertex shader
        int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);
        // check for shader compile errors

        // fragment shader
        int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);
        // check for shader compile errors

        // link shaders
        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);
        // check for linking errors

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }
    virtual ~GLDebugDrawer()
    {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteProgram(shaderProgram);
    }
    void   drawLine(const btVector3& from, const btVector3& to, const btVector3& color)
    {
        float vertices[] = {
            from.getX(), from.getY(), from.getZ(),
            to.getX(), to.getY(), to.getZ(),
        };

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "MVP"), 1, GL_FALSE, &MVP[0][0]);
        glUniform3fv(glGetUniformLocation(shaderProgram, "color"), 1, &color.m_floats[0]);

        glBindVertexArray(VAO);
        glDrawArrays(GL_LINES, 0, 2);
    }
    void    drawContactPoint(const btVector3& PointOnB,const btVector3& normalOnB,btScalar distance,int lifeTime,const btVector3& color){
        drawSphere(PointOnB, 0.1, color);
        drawLine(PointOnB, PointOnB+normalOnB.normalized()*0.5, color);
    }
    void   reportErrorWarning(const char* warningString)
    {
        std::cout << "<------BulletPhysics Debug------>\n" << warningString << "\n";
    }

    void   draw3dText(const btVector3& location, const char* textString)
    {
        std::cout << "<------BulletPhysics Debug------>\n" << textString << "\n";
    }

    void   setDebugMode(int debugMode)
    {
        m_debugMode = debugMode;
    }

    int    getDebugMode() const
    {
        return m_debugMode;
    }
    int setMVP(glm::mat4 mvp)
    {
        MVP = mvp;
        return 1;
    }
};

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

    // Create and compile our GLSL program from the shaders
    GLuint geomProgramID = LoadShaders("geom.vert", "geom.frag");

    // grab uniforms to modify
    GLuint u_time = glGetUniformLocation(geomProgramID, "time");
    GLuint u_MVP = glGetUniformLocation(geomProgramID, "MVP");
    GLuint u_model = glGetUniformLocation(geomProgramID, "model");
    GLuint u_dir_light_position = glGetUniformLocation(geomProgramID, "dirLightPos");
    GLuint u_dir_light_color = glGetUniformLocation(geomProgramID, "dirLightColor");
    GLuint u_diffuse_map = glGetUniformLocation(geomProgramID, "diffuseMap");
    GLuint u_normal_map = glGetUniformLocation(geomProgramID, "normalMap");

    // Projection matrix : 45 Field of View, screen ratio, display range : 0.1 unit <-> 100 units
    // Scale to window size
    glm::mat4 Projection;
    GBuffer gbuffer;
    {
        GLint windowWidth, windowHeight;
	    glfwGetWindowSize(window, &windowWidth, &windowHeight);
        gbuffer = generate_gbuffer(windowWidth, windowHeight);
        Projection = glm::perspective(glm::radians(45.0f), (float)windowWidth/(float)windowHeight, 0.1f, 100.0f);
    }
	
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

    // Camera
    auto const cameraUp = glm::vec3(0,1,0);
    glm::vec3 cameraPosition = glm::normalize(glm::vec3(4, 3, -3));
    glm::vec3 cameraPivot = glm::vec3(0,0,0);
    float cameraDistance = glm::length(cameraPosition - cameraPivot);
    glm::mat4 View = glm::lookAt(
                         cameraPosition * cameraDistance, // Camera is at (4,3,-3), in World Space
                         glm::vec3(0, 0, 0), // and looks at the origin
                         glm::vec3(0, 1, 0)  // Head is up (set to 0,-1,0 to look upside-down)
                     );
    glm::vec3 sun_position = glm::vec3(-0.7071067811865475, -0.7071067811865475, 0);
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
        load_asset(asset, path+".obj", path+".mtl");
        asset->name = path;
        asset->programID = geomProgramID;
        asset->drawMode = GL_TRIANGLES;
        asset->drawStart = 0;
        asset->drawType = GL_UNSIGNED_SHORT;

        assets.push_back(asset);
    }
    GLuint default_tDiffuse = loadImage("default_diffuse.bmp");
    GLuint default_tNormal  = loadImage("default_normal.bmp");
    
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

    // Background
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    
    // create a file browser instance
    ImGui::FileBrowser fileDialog;
    std::string fileDialogType = "";

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version.c_str());

    // Define movement state
    CameraState cameraState = TRACKBALL; 

    // Scale to window size
    GLint oldWindowWidth, oldWindowHeight;
    glfwGetWindowSize(window, &oldWindowWidth, &oldWindowHeight);

    double old_xpos, old_ypos;

    int selected_entity = -1;
    do {
        // Might be jank
        // Scale to window size
        GLint windowWidth, windowHeight;
        glfwGetWindowSize(window, &windowWidth, &windowHeight);
        if (windowWidth != oldWindowWidth || windowHeight != oldWindowHeight){
            Projection = glm::perspective(glm::radians(45.0f), (float)windowWidth/(float)windowHeight, 0.1f, 100.0f);
        }
        oldWindowWidth = windowWidth;
        oldWindowHeight = windowHeight;

        static double lastTime = glfwGetTime();
        double currentTime = glfwGetTime();
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        // Process Events
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        static int oldKeySpaceState = GLFW_RELEASE;
        if(glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && oldKeySpaceState == GLFW_RELEASE){
            if(cameraState == TRACKBALL) {
                cameraState = SHOOTER;

                // Reset mouse position for next frame
                xpos = (float)windowWidth/2;
                ypos = (float)windowHeight/2;

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
                    windowWidth, windowHeight,
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
                float deltaAngleX = (2 * M_PI / (float)windowWidth); // a movement from left to right = 2*PI = 360 deg
                float deltaAngleY = (M_PI / (float)windowHeight);  // a movement from top to bottom = PI = 180 deg
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
            glfwSetCursorPos(window, (float)windowWidth/2, (float)windowHeight/2);
            
            glm::vec3 cameraDirection = glm::normalize(cameraPivot - cameraPosition);

            // step 1 : Calculate the amount of rotation given the mouse movement.
            float deltaAngleX = (2 * M_PI / (float)windowWidth); // a movement from left to right = 2*PI = 360 deg
            float deltaAngleY = (M_PI / (float)windowHeight);  // a movement from top to bottom = PI = 180 deg
                                                               //
            float xAngle = (float)((float)windowWidth/2 -  xpos) * deltaAngleX;
            float yAngle = (float)((float)windowHeight/2 - ypos) * deltaAngleY;

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
                cameraPosition += cameraDirection * deltaTime * cameraMovementSpeed;
            }
            // Move backward
            if (glfwGetKey( window, GLFW_KEY_DOWN ) == GLFW_PRESS){
                cameraPosition -= cameraDirection * deltaTime * cameraMovementSpeed;
            }
            // Strafe right
            if (glfwGetKey( window, GLFW_KEY_RIGHT ) == GLFW_PRESS){
                cameraPosition += cameraRight * deltaTime * cameraMovementSpeed;
            }
            // Strafe left
            if (glfwGetKey( window, GLFW_KEY_LEFT ) == GLFW_PRESS){
                cameraPosition -= cameraRight * deltaTime * cameraMovementSpeed;
            }

            View = glm::lookAt(cameraPosition, cameraPosition + cameraDirectionRotated, cameraUp);
            cameraPivot = cameraPosition + cameraDirectionRotated;
        }

        // Update the mouse position for the next rotation
        old_xpos = xpos;
        old_ypos = ypos;

        //dynamicsWorld->stepSimulation(deltaTime);

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
 
        // Bind gbuffer to write during geometry pass
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gbuffer.fbo);

        // Only the geometry pass updates the depth buffer
        glDepthMask(GL_TRUE);

        glClearColor(clear_color.x,clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        for (auto entity : entities) {
            if(entity == nullptr)
                continue;

            ModelAsset* asset = entity->asset;
            glUseProgram(asset->programID);

            glUniform1f(u_time, (float)currentTime);

            auto MVP = Projection * View * entity->transform;

            glUniformMatrix4fv(u_MVP, 1, GL_FALSE, &MVP[0][0]);
            glUniformMatrix4fv(u_model, 1, GL_FALSE, &entity->transform[0][0]);
            glUniform3fv(u_dir_light_position, 1, &sun_position[0]);
            glUniform3fv(u_dir_light_color, 1, &sun_color[0]);
            
            glActiveTexture(GL_TEXTURE0);
            if(asset->mat->tDiffuse != GL_FALSE){
                glBindTexture(GL_TEXTURE_2D, asset->mat->tDiffuse);
            } else {
                glBindTexture(GL_TEXTURE_2D, default_tDiffuse);
            }
            glUniform1i(u_diffuse_map, 0);

            glActiveTexture(GL_TEXTURE1);
            if(asset->mat->tDiffuse != GL_FALSE){
                glBindTexture(GL_TEXTURE_2D, asset->mat->tNormal);
            } else {
                glBindTexture(GL_TEXTURE_2D, default_tNormal);
            }
            glUniform1i(u_normal_map, 1);

            //bind VAO and draw
            glBindVertexArray(asset->vao);
            glDrawElements(asset->drawMode, asset->drawCount, asset->drawType, (void*)asset->drawStart);
        }
        glBindVertexArray(0);
        glUseProgram(0);
        glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);

        // Perform lighting pass from gbuffer
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_ONE, GL_ONE);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        for (unsigned int i = 0 ; i < GBuffer::GBUFFER_NUM_TEXTURES; i++) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, gbuffer.textures[GBuffer::GBUFFER_TEXTURE_TYPE_POSITION + i]);
        }
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw bt debug
        //btDebugDrawer.setMVP(Projection * View);
        //dynamicsWorld->debugDrawWorld();

        // Start the Dear ImGui fram();
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
            ImGui::InputFloat3("Sun direction", (float*)&sun_position);
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
                ImGui::Image((void *)(intptr_t)gbuffer.textures[GBuffer::GBUFFER_TEXTURE_TYPE_DIFFUSE], ImVec2((int)windowWidth/10, (int)windowHeight/10));
                ImGui::SameLine();
                ImGui::Image((void *)(intptr_t)gbuffer.textures[GBuffer::GBUFFER_TEXTURE_TYPE_NORMAL], ImVec2((int)windowWidth/10, (int)windowHeight/10));
                ImGui::Image((void *)(intptr_t)gbuffer.textures[GBuffer::GBUFFER_TEXTURE_TYPE_TEXCOORD], ImVec2((int)windowWidth/10, (int)windowHeight/10));
                ImGui::SameLine();
                ImGui::Image((void *)(intptr_t)gbuffer.textures[GBuffer::GBUFFER_TEXTURE_TYPE_POSITION], ImVec2((int)windowWidth/10, (int)windowHeight/10));
            }
            ImGui::End();
            fileDialog.Display();
            if(fileDialog.HasSelected())
            {
                std::cout << "Selected filename: " << fileDialog.GetSelected().string() << std::endl;
                if(fileDialogType == "asset.mat.tDiffuse"){
                    if(selected_entity != -1)
                        entities[selected_entity]->asset->mat->tDiffuse = loadImage(fileDialog.GetSelected().string());
                }
                else if(fileDialogType == "asset.mat.tNormal"){
                    if(selected_entity != -1)
                        entities[selected_entity]->asset->mat->tNormal = loadImage(fileDialog.GetSelected().string());
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


