#ifndef EDITOR_H
#define EDITOR_H

#include <string>
#include <iostream>
#include <stack>

#include <glm/glm.hpp>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imfilebrowser.hpp"
#include "graphics.hpp"
#include "utilities.hpp"
#include "entities.hpp"

enum class TransformType : unsigned int{
    POS = 1,
    ROT = 2,
    SCL = 4,
    POS_ROT = 3,
    POS_SCL = 5,
    ROT_SCL = 6,
    ALL = 7
};

void initEditorGui();

void drawEditorGui(Camera &camera, EntityManager &entity_manager, std::vector<Asset *> &assets);

bool editorTranslationGizmo(glm::vec3 &pos, glm::quat &rot, glm::mat3 &scl, Camera &camera, const glm::vec3 &snap, bool do_snap);
bool editorRotationGizmo(glm::vec3 &pos, glm::quat &rot, glm::mat3 &scl, const Camera &camera);
bool editorScalingGizmo(glm::vec3 &pos, glm::quat &rot, glm::mat3 &scl, Camera &camera, const glm::vec3 &snap, bool do_snap);
bool editTransform(Camera &camera, glm::vec3 &pos, glm::quat &rot, glm::mat3 &scl, TransformType type);
void drawEditor3DArrow(const glm::vec3 &position, const glm::vec3 &direction, const Camera &camera, const glm::vec4 &color, const glm::vec3 &scale, bool shaded=true, bool block=false);
void drawEditor3DRing(const glm::vec3 &position, const glm::vec3 &direction, const Camera &camera, const glm::vec4 &color, const glm::vec3 &scale, bool shaded=true);
void drawMeshCube(const glm::vec3 &pos, const glm::quat &rot, const glm::mat3x3 &scl, const Camera &camera);
void drawMeshWireframe(Mesh *mesh, const glm::vec3 &pos, const glm::quat &rot, const glm::mat3x3 &scl, const Camera &camera, bool flash);
void drawWaterDebug(WaterEntity* w_e, const Camera &camera, bool flash);
//class GLDebugDrawer : public btIDebugDraw {
//    GLuint shaderProgram;
//    glm::mat4 MVP;
//    GLuint VBO, VAO;
//    int m_debugMode;
//
//public:
//    GLDebugDrawer(){};
//    void init(){
//        MVP = glm::mat4(1.0f);
//
//        const char *vertexShaderSource = "#version 330 core\n"
//                                         "layout (location = 0) in vec3 aPos;\n"
//                                         "uniform mat4 MVP;\n"
//                                         "void main()\n"
//                                         "{\n"
//                                         "   gl_Position = MVP * vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
//                                         "}\0";
//        const char *fragmentShaderSource = "#version 330 core\n"
//                                           "out vec4 FragColor;\n"
//                                           "uniform vec3 color;\n"
//                                           "void main()\n"
//                                           "{\n"
//                                           "   FragColor = vec4(color, 1.0f);\n"
//                                           "}\n\0";
//
//        // vertex shader
//        int vertexShader = glCreateShader(GL_VERTEX_SHADER);
//        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
//        glCompileShader(vertexShader);
//        // check for shader compile errors
//
//        // fragment shader
//        int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
//        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
//        glCompileShader(fragmentShader);
//        // check for shader compile errors
//
//        // link shaders
//        shaderProgram = glCreateProgram();
//        glAttachShader(shaderProgram, vertexShader);
//        glAttachShader(shaderProgram, fragmentShader);
//        glLinkProgram(shaderProgram);
//        // check for linking errors
//
//        glDeleteShader(vertexShader);
//        glDeleteShader(fragmentShader);
//    }
//    virtual ~GLDebugDrawer()
//    {
//        glDeleteVertexArrays(1, &VAO);
//        glDeleteBuffers(1, &VBO);
//        glDeleteProgram(shaderProgram);
//    }
//    void   drawLine(const btVector3& from, const btVector3& to, const btVector3& color)
//    {
//        float vertices[] = {
//            from.getX(), from.getY(), from.getZ(),
//            to.getX(), to.getY(), to.getZ(),
//        };
//
//        glGenVertexArrays(1, &VAO);
//        glGenBuffers(1, &VBO);
//        glBindVertexArray(VAO);
//
//        glBindBuffer(GL_ARRAY_BUFFER, VBO);
//        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices, GL_STATIC_DRAW);
//
//        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
//        glEnableVertexAttribArray(0);
//
//        glBindBuffer(GL_ARRAY_BUFFER, 0);
//        glBindVertexArray(0);
//
//        glUseProgram(shaderProgram);
//        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "MVP"), 1, GL_FALSE, &MVP[0][0]);
//        glUniform3fv(glGetUniformLocation(shaderProgram, "color"), 1, &color.m_floats[0]);
//
//        glBindVertexArray(VAO);
//        glDrawArrays(GL_LINES, 0, 2);
//    }
//    void    drawContactPoint(const btVector3& PointOnB,const btVector3& normalOnB,btScalar distance,int lifeTime,const btVector3& color){
//        drawSphere(PointOnB, 0.1, color);
//        drawLine(PointOnB, PointOnB+normalOnB.normalized()*0.5, color);
//    }
//    void   reportErrorWarning(const char* warningString)
//    {
//        std::cout << "<------BulletPhysics Debug------>\n" << warningString << "\n";
//    }
//
//    void   draw3dText(const btVector3& location, const char* textString)
//    {
//        std::cout << "<------BulletPhysics Debug------>\n" << textString << "\n";
//    }
//
//    void   setDebugMode(int debugMode)
//    {
//        m_debugMode = debugMode;
//    }
//
//    int    getDebugMode() const
//    {
//        return m_debugMode;
//    }
//    int setMVP(glm::mat4 mvp)
//    {
//        MVP = mvp;
//        return 1;
//    }
//};

namespace editor {
    extern enum GizmoMode {
        GIZMO_MODE_TRANSLATE = 0,
        GIZMO_MODE_ROTATE,
        GIZMO_MODE_SCALE,
        GIZMO_MODE_NONE,
    } gizmo_mode;
    extern std::string im_file_dialog_type;
    //extern GLDebugDrawer bt_debug_drawer;
    //extern bool draw_bt_debug;
    extern ImGui::FileBrowser im_file_dialog;
    extern bool draw_debug_wireframe;
    extern bool transform_active;
    extern Id sel_e;
}

#endif
