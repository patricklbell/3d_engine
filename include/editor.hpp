#ifndef EDITOR_H
#define EDITOR_H
bool edit_transform(float* cameraView, float* cameraProjection, float* matrix, float cameraDistance);

class GLDebugDrawer : public btIDebugDraw {
    GLuint shaderProgram;
    glm::mat4 MVP;
    GLuint VBO, VAO;
    int m_debugMode;

public:
    GLDebugDrawer()
    virtual ~GLDebugDrawer();
    void drawLine(const btVector3& from, const btVector3& to, const btVector3& color);
    void drawContactPoint(const btVector3& PointOnB,const btVector3& normalOnB,btScalar distance,int lifeTime,const btVector3& color);
    void   reportErrorWarning(const char* warningString);
    void   draw3dText(const btVector3& location, const char* textString);
    void   setDebugMode(int debugMode);
    int    getDebugMode();
    int setMVP(glm::mat4 mvp);
};

#endif
