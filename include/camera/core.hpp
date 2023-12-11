#ifndef ENGINE_CAMERA_CORE_HPP
#define ENGINE_CAMERA_CORE_HPP

#include <glm/glm.hpp>
#include <utilities/math.hpp>

struct Frustrum {
    float near_plane = 0.1f, far_plane = 100.0f;
    float fov_y = glm::radians(45.0f);
    float aspect_ratio = 1.0; // width / height
};

struct Camera {
    enum Type : uint8_t {
        TRACKBALL = 0,
        SHOOTER = 1,
        STATIC = 2,
    } state = Type::STATIC;

    Camera(Frustrum _frustrum = Frustrum(), glm::vec3 _position = glm::vec3(3.0), glm::vec3 _target = glm::vec3(0.0));
    void set_frustrum(Frustrum _frustrum = Frustrum());
    void set_position(glm::vec3 _position);
    void set_target(glm::vec3 _target);
    void set_aspect_ratio(float aspect_ratio = 1.0f);
    bool update(); // Returns true if any update to the matrices occured

    Frustrum frustrum;

    glm::vec3 up = glm::vec3(0, 1, 0);
    glm::vec3 position;
    glm::vec3 target;


    glm::vec3 world_up;
    glm::vec3 forward;
    glm::vec3 right;

    bool view_updated = false;
    bool projection_updated = false;
    // These are set by the update method
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 vp;
    glm::mat4 inv_vp;
};

struct FrustrumCollider {
    /*glm::vec3 n_near, n_far, n_right, n_left, n_top, n_bottom;
    glm::vec3 p_ntr, p_nbr, p_ntl, p_nbl, p_ftr, p_fbr, p_ftl, p_fbl;*/
    glm::vec3 points[8];
    glm::vec4 planes[6];

    FrustrumCollider(const struct Camera& cam);
    bool isAabbInFrustrum(AABB& aabb);
};

#endif // ENGINE_CAMERA_CORE_HPP