#include <limits>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/detail/type_mat.hpp>
#include <glm/detail/type_vec.hpp>
#include <glm/fwd.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <utilities/math.hpp>

// @perf These could probably be made more efficient
glm::mat4x4 createModelMatrix(const glm::vec3& pos, const glm::quat& rot, const glm::mat3x3& scl) {
    return glm::translate(glm::mat4x4(1.0), pos) * glm::mat4_cast(rot) * glm::mat4x4(scl);
}

glm::mat4x4 createModelMatrix(const glm::vec3& pos, const glm::mat3x3& rot, const glm::mat3x3& scl) {
    return glm::translate(glm::mat4x4(1.0), pos) * glm::mat4x4(rot) * glm::mat4x4(scl);
}

glm::mat4x4 createModelMatrix(const glm::vec3& pos, const glm::mat3x3& rot, const glm::vec3& scl) {
    return glm::translate(glm::mat4x4(1.0), pos) * glm::mat4x4(rot) * glm::scale(glm::mat4(1.0f), scl);
}

glm::mat4x4 createModelMatrix(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scl) {
    return glm::translate(glm::mat4x4(1.0), pos) * glm::mat4_cast(rot) * glm::scale(glm::mat4(1.0f), scl);
}

glm::mat4x4 createModelMatrix(const glm::mat4& t, const glm::vec3& pos, const glm::quat& rot, const glm::mat3& scl) {
    return glm::translate(glm::mat4(1.0), pos) * t * glm::mat4_cast(rot) * glm::mat4x4(scl);
}

glm::mat4x4 lerpMatrix(glm::mat4& m1, glm::mat4& m2, float t) {
    glm::vec3 scale1;
    glm::quat rotation1;
    glm::vec3 translation1;
    glm::vec3 skew1;
    glm::vec4 perspective1;
    glm::decompose(m1, scale1, rotation1, translation1, skew1, perspective1);
    //std::cout << "lerping matrices: \n\tscale: " << scale1 << "\n\tposition: " << translation1 << "\n\trotation: " << rotation1 << "\n";

    glm::vec3 scale2;
    glm::quat rotation2;
    glm::vec3 translation2;
    glm::vec3 skew2;
    glm::vec4 perspective2;
    glm::decompose(m2, scale2, rotation2, translation2, skew2, perspective2);
    //std::cout << "-->\n\tscale: " << scale2 << "\n\tposition: " << translation2 << "\n\trotation: " << rotation2 << "\n";

    glm::vec3 scale = glm::mix(scale1, scale2, t);
    glm::quat rotation = glm::slerp(glm::conjugate(rotation1), glm::conjugate(rotation2), t);
    glm::vec3 translation = glm::mix(translation1, translation2, t);
    // @todo skew, perspective
    //glm::vec3 skew = glm::mix(skew1, skew2, t);
    //glm::vec4 perspective = glm::mix(perspective1, perspective2, t);
    return createModelMatrix(translation, rotation, scale);
}

glm::vec3 getScaleMatrix(glm::mat4& m) {
    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(m, scale, rotation, translation, skew, perspective);
    return scale;
}

// https://nelari.us/post/gizmos/, determine change quaternion from the difference between two points,
// returns angle in radians from a to b
float angleBetweenDirections(const glm::vec3& a, const glm::vec3& b, const glm::vec3& n) {
    float angle = glm::abs(glm::acos(glm::clamp(glm::dot(a, b), -1.0f, 1.0f)));
    angle *= glm::sign(glm::dot(glm::cross(a, b), n));
    return angle;
}

// https://cadxfem.org/inf/Fast%20MinimumStorage%20RayTriangle%20Intersection.pdf
// Cornell university paper describing ray intersection algorithms
static const double epsilon = 0.000001;
bool raycastTriangle(const glm::vec3 vertices[3], Raycast& raycast) {
    glm::vec3 edge1 = vertices[1] - vertices[0];
    glm::vec3 edge2 = vertices[2] - vertices[0];
    glm::vec3 p = glm::cross(raycast.direction, edge2);
    float det = glm::dot(edge1, p);
    if (det < epsilon && det > -epsilon)
        return false;

    float inv_det = 1.0f / det;

    auto t = raycast.origin - vertices[0];
    float u = glm::dot(t, p) * inv_det;
    if (u < 0.0 || u > 1.0)
        return false;

    auto q = glm::cross(t, edge1);

    float v = glm::dot(raycast.direction, q) * inv_det;
    if (v < 0.0 || u + v > 1.0)
        return false;

    raycast.result.t = glm::dot(edge2, q) * inv_det; // @note This could be skipped an intersection test is needed
    raycast.result.u = u;
    raycast.result.v = v;
    raycast.result.hit = true;
    return true;
}

bool raycastTriangleCull(const glm::vec3 vertices[3], Raycast& raycast) {
    auto edge1 = vertices[1] - vertices[0];
    auto edge2 = vertices[2] - vertices[0];
    auto p = glm::cross(raycast.direction, edge2);
    float det = glm::dot(edge1, p);
    if (det < epsilon) return false;

    auto t_vec = raycast.origin - vertices[0];
    float u = glm::dot(t_vec, p);
    if (u < 0.0 || u > det) return false;

    auto q = glm::cross(t_vec, edge1);
    float v = glm::dot(raycast.direction, q);
    if (v < 0.0 || u + v > det) return false;

    float t = glm::dot(edge2, q);
    float inv_det = 1.0f / det;
    t *= inv_det;
    u *= inv_det;
    v *= inv_det;

    raycast.result.t = t; // @note This could be skipped an intersection test is needed
    raycast.result.u = u;
    raycast.result.v = v;
    raycast.result.hit = true;
    return true;
}

bool raycastTriangles(glm::vec3* vertices, unsigned int* indices, const int num_indices, const glm::mat4x4& model, Raycast& raycast) {
    Raycast tri_raycast = raycast;

    glm::vec3 triangle[3];
    for (int j = 0; j < num_indices; j += 3) {
        const auto& p1 = vertices[indices[j  ]];
        const auto& p2 = vertices[indices[j+1]];
        const auto& p3 = vertices[indices[j+2]];

        triangle[0] = glm::vec3(model * glm::vec4(p1, 1.0));
        triangle[1] = glm::vec3(model * glm::vec4(p2, 1.0));
        triangle[2] = glm::vec3(model * glm::vec4(p3, 1.0));
        if (raycastTriangle(triangle, tri_raycast)) {
            if (tri_raycast.result.t < raycast.result.t) {
                raycast = tri_raycast;
            }
        }
    }
    return raycast.result.hit;
}

bool raycastTrianglesTest(glm::vec3* vertices, unsigned int* indices, const int num_indices, const glm::mat4x4& vertices_transform, Raycast& raycast) {
    for (int j = 0; j < num_indices; j += 3) {
        const auto p1 = vertices[indices[j]];
        const auto p2 = vertices[indices[j + 1]];
        const auto p3 = vertices[indices[j + 2]];
        glm::vec3 triangle[3] = {
            glm::vec3(vertices_transform * glm::vec4(p1, 1.0)),
            glm::vec3(vertices_transform * glm::vec4(p2, 1.0)),
            glm::vec3(vertices_transform * glm::vec4(p3, 1.0))
        };
        if (raycastTriangle(triangle, raycast)) {
            return true;
        }
    }
    return false;
}

bool raycastPlane(const glm::vec3& center, const glm::vec3& normal, Raycast& raycast) {
    float pn_ld = glm::dot(normal, raycast.direction);
    // Line parallel to plane
    if (glm::abs(pn_ld) < epsilon) return false;

    // Napkin math justification:
    // p = lo + t*ld
    // pn . (p - po) = 0
    // pn . (lo + t*ld - po) = 0
    // t*(pn . ld) = pn . (po - lo)
    // t = pn . (po - lo) / (pn . ld)
    raycast.result.t = glm::dot(normal, center - raycast.origin) / pn_ld;
    raycast.result.normal = normal;
    raycast.result.hit = true;

    return true;
}

// Bounds is the distance from center to the edge in each direction
bool raycastBoundedPlane(const glm::vec3& center, const glm::vec3& normal, const glm::vec3& bounds, Raycast& raycast) {
    if (raycastPlane(center, normal, raycast)) {
        auto P = raycast.origin + raycast.result.t * raycast.direction;
        auto O = glm::abs(P - center);

        // Make sure point lies within face
        if (glm::any(glm::greaterThan(O, bounds))) {
            raycast.result.hit = false;
        }
    }
    return raycast.result.hit;
}

bool raycastCube(const glm::vec3& center, const glm::vec3& scale, Raycast& raycast) {
    Raycast plane_raycast = raycast;

    // Loop through each face and calculate the intersection point with a ray
    for (int i = 0; i < 3; i++) {
        for (int d = -1; d <= 1; d += 2) {
            auto face_N = glm::vec3(d * (i == 0), d * (i == 1), d * (i == 2));
            auto face_P = center + scale * face_N;

            if (raycastBoundedPlane(face_P, face_N, scale/2.0f, plane_raycast)) {
                if (plane_raycast.result.t < raycast.result.t) {
                    raycast = plane_raycast;
                    raycast.result.normal = face_N;
                }
            }
        }
    }
    return raycast.result.hit;
}

bool raycastAabb(const AABB& aabb, Raycast& raycast) {
    auto min = aabb.center - aabb.size, max = aabb.center + aabb.size;

    auto t1 = (min - raycast.origin) / raycast.direction;
    auto t2 = (max - raycast.origin) / raycast.direction;

    auto tmin = glm::max(glm::max(glm::min(t1.x, t2.x), glm::min(t1.y, t2.y)), glm::min(t1.z, t2.z));
    auto tmax = glm::min(glm::min(glm::max(t1.x, t2.x), glm::max(t1.y, t2.y)), glm::max(t1.z, t2.z));

    raycast.result.hit = tmax >= 0 && tmax >= tmin;
    raycast.result.t = tmin;
    return raycast.result.hit;
}

float distanceBetweenLines(const glm::vec3& l1_origin, const glm::vec3& l1_direction, const glm::vec3& l2_origin, const glm::vec3& l2_direction, float& l1_t, float& l2_t)
{
    const glm::vec3 dp = l2_origin - l1_origin;
    const float v12 = dot(l1_direction, l1_direction);
    const float v22 = dot(l2_direction, l2_direction);
    const float v1v2 = dot(l1_direction, l2_direction);

    const float det = v1v2 * v1v2 - v12 * v22;

    if (glm::abs(det) > FLT_MIN)
    {
        const float inv_det = 1.f / det;

        const float dpv1 = dot(dp, l1_direction);
        const float dpv2 = dot(dp, l2_direction);

        l1_t = inv_det * (v22 * dpv1 - v1v2 * dpv2);
        l2_t = inv_det * (v1v2 * dpv1 - v12 * dpv2);

        return glm::length(dp + l2_direction * l2_t - l1_direction * l1_t);
    }
    else
    {
        const glm::vec3 a = glm::cross(dp, l1_direction);
        return std::sqrt(dot(a, a) / v12);
    }
}

Raycast mouseToRaycast(glm::ivec2 mouse_position, glm::ivec2 screen_size, glm::mat4 inv_vp) {
    // The ray Start and End positions, in Normalized Device Coordinates
    glm::vec4 ray_start_NDC(
        ((float)mouse_position.x / (float)screen_size.x - 0.5f) * 2.0f, // [0,1024] -> [-1,1]
        -((float)mouse_position.y / (float)screen_size.y - 0.5f) * 2.0f, // [0, 768] -> [-1,1]
        -1.0, // The near plane maps to Z=-1 in Normalized Device Coordinates
        1.0f
    );
    glm::vec4 ray_end_NDC(
        ((float)mouse_position.x / (float)screen_size.x - 0.5f) * 2.0f,
        -((float)mouse_position.y / (float)screen_size.y - 0.5f) * 2.0f,
        0.0,
        1.0f
    );
    // Inverse of projection and view to obtain NDC to world
    glm::vec4 ray_start_world = inv_vp * ray_start_NDC;
    ray_start_world /= ray_start_world.w;
    glm::vec4 ray_end_world = inv_vp * ray_end_NDC;
    ray_end_world /= ray_end_world.w;

    return Raycast(glm::vec3(ray_start_world), glm::normalize(glm::vec3(ray_end_world - ray_start_world)));
}

AABB&& transformAABB(AABB& aabb, glm::mat4 transform) {
    // Translate center
    const glm::vec3 t_center{ transform * glm::vec4(aabb.center, 1.f) };

    // Scaled orientation
    const auto& right = glm::vec3(transform[0]) * aabb.size.x;
    const auto& up = glm::vec3(transform[1]) * aabb.size.y;
    const auto& forward = glm::vec3(transform[2]) * aabb.size.z;

    const float t_x = std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, right)) +
        std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, up)) +
        std::abs(glm::dot(glm::vec3{ 1.f, 0.f, 0.f }, forward));

    const float t_y = std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, right)) +
        std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, up)) +
        std::abs(glm::dot(glm::vec3{ 0.f, 1.f, 0.f }, forward));

    const float t_z = std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, right)) +
        std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, up)) +
        std::abs(glm::dot(glm::vec3{ 0.f, 0.f, 1.f }, forward));

    return AABB{ t_center, glm::vec3(t_x, t_y, t_z) };
}