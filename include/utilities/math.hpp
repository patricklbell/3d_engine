#ifndef UTILITIES_MATH_HPP
#define UTILITIES_MATH_HPP

#include <ostream>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/common.hpp>

glm::mat4x4 createModelMatrix(const glm::vec3& pos, const glm::quat& rot, const glm::mat3x3& scl);
glm::mat4x4 createModelMatrix(const glm::vec3& pos, const glm::mat3x3& rot, const glm::mat3x3& scl);
glm::mat4x4 createModelMatrix(const glm::vec3& pos, const glm::mat3x3& rot, const glm::vec3& scl);
glm::mat4x4 createModelMatrix(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scl);
glm::mat4x4 createModelMatrix(const glm::mat4& t, const glm::vec3& pos, const glm::quat& rot, const glm::mat3& scl);

glm::mat4x4 lerpMatrix(glm::mat4& m1, glm::mat4& m2, float t);
glm::vec3 getScaleMatrix(glm::mat4& m);
float angleBetweenDirections(const glm::vec3& a, const glm::vec3& b, const glm::vec3& n);

struct Raycast {
    glm::vec3 origin;
    glm::vec3 direction;

    Raycast(glm::vec3 _origin, glm::vec3 _direction) : origin(_origin), direction(_direction) {}
};

// This contains the result of a raycast, anything other than hit and t is not garaunteed to be updated
struct RaycastResult {
    bool hit = false;
    operator bool() const { return hit;  }

    unsigned int indice; // @note this is the actual array index, so you don't need to multiply by 3
    float t = FLT_MAX, u = -1.0, v = -1.0; // This distance to the intersection, the uv coordinates on the triangle
    glm::vec3 normal;
};

struct AABB {
    glm::vec3 center;
    glm::vec3 size;
    static AABB&& FromMinMax(glm::vec3 min, glm::vec3 max) {
        return AABB{ (max + min) * 0.5f, (max - min) * 0.5f };
    }
};
AABB&& transformAABB(AABB& aabb, glm::mat4 transform);

Raycast mouseToRaycast(glm::ivec2 mouse_position, glm::ivec2 screen_size, glm::mat4 inv_vp);
RaycastResult raycastTriangleCull(const glm::vec3 vertices[3], Raycast& raycast);
RaycastResult raycastTriangle(const glm::vec3 vertices[3], Raycast& raycast);
RaycastResult raycastTriangles(glm::vec3* vertices, unsigned int* indices, const int num_indices, const glm::mat4x4& vertices_transform, Raycast& raycast);
RaycastResult raycastTrianglesTest(glm::vec3* vertices, unsigned int* indices, const int num_indices, const glm::mat4x4& vertices_transform, Raycast& raycast);
RaycastResult raycastPlane(const glm::vec3& center, const glm::vec3& normal, Raycast& raycast);
RaycastResult raycastBoundedPlane(const glm::vec3& center, const glm::vec3& normal, const glm::vec3& bounds, Raycast& raycast);
RaycastResult raycastCube(const glm::vec3& center, const glm::vec3& scale, Raycast& raycast);
RaycastResult raycastAabb(const AABB& aabb, Raycast& raycast);
RaycastResult raycastSphere(const glm::vec3& center, const float scale, Raycast& raycast);

float distanceBetweenLines(const glm::vec3& l1_origin, const glm::vec3& l1_direction, const glm::vec3& l2_origin, const glm::vec3& l2_direction, float& l1_t, float& l2_t);
float distanceToAabb(const AABB& aabb, glm::vec3& point);

// ------------ constexpr function implementations ------------ 
constexpr uint64_t log2(uint64_t n) {
    return ((n < 2) ? 1 : 1 + log2(n / 2));
}

constexpr uint64_t nextPowerOf2(int tex_x, int tex_y) {
    uint64_t v = std::max(tex_x, tex_y);

    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    v++;

    return log2(v);
}

// Stream operators for some GLM types
template<typename T, glm::precision P>
std::ostream& operator<<(std::ostream& os, const glm::tvec1<T, P>& v) {
    return os << v.x;
}

template<typename T, glm::precision P>
std::ostream& operator<<(std::ostream& os, const glm::tvec2<T, P>& v) {
    return os << v.x << ", " << v.y;
}

template<typename T, glm::precision P>
std::ostream& operator<<(std::ostream& os, const glm::tvec3<T, P>& v) {
    return os << v.x << ", " << v.y << ", " << v.z;
}

template<typename T, glm::precision P>
std::ostream& operator<<(std::ostream& os, const glm::tvec4<T, P>& v) {
    return os << v.x << ", " << v.y << ", " << v.z << ", " << v.w;
}

template<typename T, glm::precision P>
std::ostream& operator<<(std::ostream& os, const glm::tquat<T, P>& v) {
    return os << v.x << ", " << v.y << ", " << v.z << ", " << v.w;
}

template<typename T, glm::precision P>
std::ostream& operator<<(std::ostream& os, const glm::tmat4x4<T, P>& m) {
    return os << "{ \n\t" << m[0][0] << ", " << m[0][1] << ", " << m[0][2] << ", " << m[0][3] << "\n\t"
        << m[1][0] << ", " << m[1][1] << ", " << m[1][2] << ", " << m[1][3] << "\n\t"
        << m[2][0] << ", " << m[2][1] << ", " << m[2][2] << ", " << m[2][3] << "\n\t"
        << m[3][0] << ", " << m[3][1] << ", " << m[3][2] << ", " << m[3][3] << "\n}";
}

template <typename genType>
GLM_FUNC_QUALIFIER genType linearstep(genType edge0, genType edge1, genType x)
{
    GLM_STATIC_ASSERT(std::numeric_limits<genType>::is_iec559, "'linearstep' only accept floating-point inputs");

    return glm::clamp((x - edge0) / (edge1 - edge0), genType(0), genType(1));
}
template <typename T, glm::precision P, template <typename, glm::precision> class vecType>
GLM_FUNC_QUALIFIER vecType<T, P> linearstep(T edge0, T edge1, vecType<T, P> const& x)
{
    GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'linearstep' only accept floating-point inputs");

    return glm::clamp((x - edge0) / (edge1 - edge0), static_cast<T>(0), static_cast<T>(1));
}
template <typename T, glm::precision P, template <typename, glm::precision> class vecType>
GLM_FUNC_QUALIFIER vecType<T, P> linearstep(vecType<T, P> const& edge0, vecType<T, P> const& edge1, vecType<T, P> const& x)
{
    GLM_STATIC_ASSERT(std::numeric_limits<T>::is_iec559, "'linearstep' only accept floating-point inputs");

    return glm::clamp((x - edge0) / (edge1 - edge0), static_cast<T>(0), static_cast<T>(1));
}

#endif // UTILITIES_MATH_HPP