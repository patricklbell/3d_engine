#ifndef CAMERA_CORE_HPP
#define CAMERA_CORE_HPP

#include <glm/glm.hpp>

struct Frustrum {
    float near_plane = 0.01f, far_plane = 100.0f;
    float fov = glm::radians(45.0f);
    float aspect_ratio = 1.0; // width / height
};

struct Camera {
    enum TYPE : uint8_t {
        TRACKBALL = 0,
        SHOOTER = 1,
        STATIC = 2,
    } state;

    Camera(Frustrum _frustrum = Frustrum(), glm::vec3 _position = glm::vec3(3.0));
    void set_frustrum(Frustrum _frustrum = Frustrum());
    void set_position(glm::vec3 _position);
    void set_target(glm::vec3 _target);
    void set_aspect_ratio(float aspect_ratio = 1.0f);
    bool update(); // Returns true if any update to the matrices occured

    Frustrum frustrum;

    glm::vec3 up = glm::vec3(0, 1, 0);
    glm::vec3 position;
    glm::vec3 target = glm::vec3(0, 0, 0);

    glm::vec3 forward;
    glm::vec3 right;

    bool view_updated = false;
    bool projection_updated = false;
    // These are set by the update method
    glm::mat4 view;
    glm::mat4 projection;
};

#endif // CAMERA_CORE_HPP
