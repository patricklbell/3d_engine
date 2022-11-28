#include <camera/core.hpp>
#include <glm/gtc/matrix_transform.hpp>

Camera::Camera(Frustrum _frustrum, glm::vec3 _position) {
    frustrum = _frustrum;
    projection_updated = true;
    view_updated = true;
}

void Camera::set_frustrum(Frustrum _frustrum) {
    frustrum = _frustrum;
    projection_updated = true;
}

void Camera::set_position(glm::vec3 _position) {
    position = _position;
    view_updated = true;
}

void Camera::set_target(glm::vec3 _target) {
    target = _target;
    view_updated = true;
}

void Camera::set_aspect_ratio(float aspect_ratio) {
    frustrum.aspect_ratio = aspect_ratio;
    projection_updated = true;
}

bool Camera::update() {
    bool update = false;
    if (view_updated) {
        view = glm::lookAt(position, target, up);
        right = glm::vec3(glm::transpose(view)[0]);
        forward = glm::normalize(position - target);
        view_updated = false;
        update = true;
    }
    if (projection_updated) {
        projection = glm::perspective(frustrum.fov, frustrum.aspect_ratio, frustrum.near_plane, frustrum.far_plane);
        projection_updated = false;
        update = true;
    }
    if (update) {
        vp = projection * view;
    }

    return update;
}