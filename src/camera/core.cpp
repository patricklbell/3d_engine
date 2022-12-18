#include <camera/core.hpp>
#include <glm/gtc/matrix_transform.hpp>

Camera::Camera(Frustrum _frustrum, glm::vec3 _position, glm::vec3 _target) {
    set_frustrum(_frustrum);
    set_position(_position);
    set_target(_target);
    update();
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
        forward = glm::normalize(target - position);
        world_up = glm::cross(right, forward);
        view_updated = false;
        update = true;
    }
    if (projection_updated) {
        projection = glm::perspective(frustrum.fov_y, frustrum.aspect_ratio, frustrum.near_plane, frustrum.far_plane);
        projection_updated = false;
        update = true;
    }
    if (update) {
        vp = projection * view;
        inv_vp = glm::inverse(vp);
    }

    return update;
}

FrustrumCollider::FrustrumCollider(const Camera& cam) {
    const float fplane_half_height = cam.frustrum.far_plane * glm::tan(cam.frustrum.fov_y * 0.5f);
    const float nplane_half_height = cam.frustrum.near_plane * glm::tan(cam.frustrum.fov_y * 0.5f);
    const float fplane_half_width  = fplane_half_height * cam.frustrum.aspect_ratio;
    const float nplane_half_width  = nplane_half_height * cam.frustrum.aspect_ratio;
    const glm::vec3 far_forward    = cam.frustrum.far_plane * cam.forward;
    const glm::vec3 near_forward   = cam.frustrum.near_plane * cam.forward;
    const glm::vec3 far_center     = cam.position + far_forward;
    const glm::vec3 near_center    = cam.position + near_forward;

    points[0] = near_center + cam.right * nplane_half_width + cam.world_up * nplane_half_height;
    points[1] = near_center + cam.right * nplane_half_width - cam.world_up * nplane_half_height;
    points[2] = near_center - cam.right * nplane_half_width + cam.world_up * nplane_half_height;
    points[3] = near_center - cam.right * nplane_half_width - cam.world_up * nplane_half_height;
    points[4] = far_center  + cam.right * fplane_half_width + cam.world_up * fplane_half_height;
    points[5] = far_center  + cam.right * fplane_half_width - cam.world_up * fplane_half_height;
    points[6] = far_center  - cam.right * fplane_half_width + cam.world_up * fplane_half_height;
    points[7] = far_center  - cam.right * fplane_half_width - cam.world_up * fplane_half_height;

    planes[0] = glm::vec4(cam.forward, 0.0);
    planes[1] = glm::vec4(-cam.forward, 0.0);
    planes[2] = glm::vec4(glm::normalize(glm::cross(cam.world_up, far_forward + cam.right * fplane_half_width)), 0.0);
    planes[3] = glm::vec4(glm::normalize(glm::cross(far_forward - cam.right * fplane_half_width, cam.world_up)), 0.0);
    planes[4] = glm::vec4(glm::normalize(glm::cross(cam.right, far_forward - cam.world_up * fplane_half_height)), 0.0);
    planes[5] = glm::vec4(glm::normalize(glm::cross(far_forward + cam.world_up * fplane_half_height, cam.right)), 0.0);

    planes[0].w = -glm::dot(glm::vec3(planes[0]), near_center);
    planes[1].w = -glm::dot(glm::vec3(planes[1]), far_center);
    planes[2].w = -glm::dot(glm::vec3(planes[2]), cam.position);
    planes[3].w = -glm::dot(glm::vec3(planes[3]), cam.position);
    planes[4].w = -glm::dot(glm::vec3(planes[4]), cam.position);
    planes[5].w = -glm::dot(glm::vec3(planes[5]), cam.position);

}

// https://iquilezles.org/articles/frustumcorrect/
//bool FrustrumCollider::isAabbInFrustrum(AABB& aabb) {
//    // @todo simd and compare with plane method
//    // check box outside/inside of frustum
//    glm::vec3 min = aabb.center - aabb.size, max = aabb.center + aabb.size; // @todo
//    for (int i = 0; i < 6; i++)
//    {
//        int out = 0;
//        out += ((glm::dot(planes[i], glm::vec4(min.x, min.y, min.z, 1.0f)) < 0.0) ? 1 : 0);
//        out += ((glm::dot(planes[i], glm::vec4(max.x, min.y, min.z, 1.0f)) < 0.0) ? 1 : 0);
//        out += ((glm::dot(planes[i], glm::vec4(min.x, max.y, min.z, 1.0f)) < 0.0) ? 1 : 0);
//        out += ((glm::dot(planes[i], glm::vec4(max.x, max.y, min.z, 1.0f)) < 0.0) ? 1 : 0);
//        out += ((glm::dot(planes[i], glm::vec4(min.x, min.y, max.z, 1.0f)) < 0.0) ? 1 : 0);
//        out += ((glm::dot(planes[i], glm::vec4(max.x, min.y, max.z, 1.0f)) < 0.0) ? 1 : 0);
//        out += ((glm::dot(planes[i], glm::vec4(min.x, max.y, max.z, 1.0f)) < 0.0) ? 1 : 0);
//        out += ((glm::dot(planes[i], glm::vec4(max.x, max.y, max.z, 1.0f)) < 0.0) ? 1 : 0);
//        if (out == 8) return false;
//    }
//
//    // check frustum outside/inside box
//    int out;
//    out = 0; for (int i = 0; i < 8; i++) out += ((points[i].x > max.x) ? 1 : 0); if (out == 8) return false;
//    out = 0; for (int i = 0; i < 8; i++) out += ((points[i].x < min.x) ? 1 : 0); if (out == 8) return false;
//    out = 0; for (int i = 0; i < 8; i++) out += ((points[i].y > max.y) ? 1 : 0); if (out == 8) return false;
//    out = 0; for (int i = 0; i < 8; i++) out += ((points[i].y < min.y) ? 1 : 0); if (out == 8) return false;
//    out = 0; for (int i = 0; i < 8; i++) out += ((points[i].z > max.z) ? 1 : 0); if (out == 8) return false;
//    out = 0; for (int i = 0; i < 8; i++) out += ((points[i].z < min.z) ? 1 : 0); if (out == 8) return false;
//
//    return true;
//}

bool isAabbForwardOfPlane(AABB& aabb, glm::vec4& plane) {
    // Geet the size of the AABB when projected onto the normal
    const float r = aabb.size.x * glm::abs(plane.x) + aabb.size.y * glm::abs(plane.y) + aabb.size.z * glm::abs(plane.z);

    return -r <= glm::dot(plane, glm::vec4(aabb.center, 1.0));
}

bool FrustrumCollider::isAabbInFrustrum(AABB& aabb) {
    return isAabbForwardOfPlane(aabb, planes[0]) &&
           isAabbForwardOfPlane(aabb, planes[1]) &&
           isAabbForwardOfPlane(aabb, planes[2]) &&
           isAabbForwardOfPlane(aabb, planes[3]) &&
           isAabbForwardOfPlane(aabb, planes[4]) &&
           isAabbForwardOfPlane(aabb, planes[5]);
}