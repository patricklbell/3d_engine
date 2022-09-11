#ifndef UTILITIES_HPP
#define UTILITIES_HPP

#include <string>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <unordered_map>

#include <glm/glm.hpp>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "graphics.hpp"

template<typename T, glm::precision P>
std::ostream& operator<<(std::ostream& os, const glm::tvec1<T, P>& v);

template<typename T, glm::precision P>
std::ostream &operator<<(std::ostream &os, const glm::tvec2<T, P> &v);

template<typename T, glm::precision P>
std::ostream &operator<<(std::ostream &os, const glm::tvec3<T, P> &v);

template<typename T, glm::precision P>
std::ostream &operator<<(std::ostream &os, const glm::tvec4<T, P> &v);

float linearstep(float e1, float e2, float x);
std::vector<std::string> split(std::string s, std::string delimiter);

void saveLevel(EntityManager& entity_manager, const std::string& level_path, const Camera &camera);
bool loadLevel(EntityManager &entity_manager, AssetManager &asset_manager, const std::string &level_path, Camera& camera);

void checkGLError(std::string identifier="");

glm::mat4x4 createModelMatrix(const glm::vec3 &pos, const glm::quat &rot, const glm::mat3x3 &scl);
glm::mat4x4 createModelMatrix(const glm::vec3& pos, const glm::mat3x3& rot, const glm::mat3x3& scl);
glm::mat4x4 createModelMatrix(const glm::vec3& pos, const glm::mat3x3& rot, const glm::vec3& scl);
glm::mat4x4 createModelMatrix(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scl);

void screenPosToWorldRay(glm::ivec2 mouse_position, glm::mat4 view, glm::mat4 projection, glm::vec3 &out_origin, glm::vec3 &out_direction);

bool rayIntersectsTriangleCull(const glm::vec3 vertices[3], const glm::vec3 &ray_origin, const glm::vec3 &ray_direction, double &t, double &u, double &v);
bool rayIntersectsTriangle(const glm::vec3 vertices[3], const glm::vec3 &ray_origin, const glm::vec3 &ray_direction, double &t, double &u, double &v);
bool rayIntersectsTriangleTestCull(const glm::vec3 vertices[3], const glm::vec3 &ray_origin, const glm::vec3 &ray_direction);
bool rayIntersectsTriangleTest(const glm::vec3 vertices[3], const glm::vec3 &ray_origin, const glm::vec3 &ray_direction);
bool rayIntersectsMesh(Mesh *mesh, const glm::mat4x4 &transform, const Camera &camera, const glm::vec3 &ray_origin, const glm::vec3 &ray_direction, glm::vec3 &collision_point, glm::vec3 &normal);
bool lineIntersectsPlane(const glm::vec3 &plane_origin, const glm::vec3 &plane_normal,const glm::vec3 &line_origin, const glm::vec3 &line_direction, float &t);
bool lineIntersectsCube(const glm::vec3& cube_position, const glm::vec3& cube_scale, const glm::vec3& line_origin, const glm::vec3& line_direction, float& t, glm::vec3 &n);

float closestDistanceBetweenLines(const glm::vec3 &l1_origin, const glm::vec3 &l1_direction, const glm::vec3 &l2_origin, const glm::vec3 &l2_direction, float &l1_t, float &l2_t);
float closestDistanceBetweenLineCircle(const glm::vec3 &line_origin, const glm::vec3 &line_direction, const glm::vec3 &circle_center, const glm::vec3 &circle_normal, float circle_radius, glm::vec3& point);

bool startsWith(std::string_view str, std::string_view prefix);
bool endsWith(std::string_view str, std::string_view suffix);
std::string getexepath();

// @note Just copied this so it might be bad
struct ThreadPool {
    void start();
    void queueJob(const std::function<void()>& job);
    void stop();
    bool busy();
    void threadLoop();

    bool should_terminate = false;           // Tells threads to stop looking for jobs
    std::mutex queue_mutex;                  // Prevents data races to the job queue
    std::condition_variable mutex_condition; // Allows threads to wait on new jobs or termination 
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> jobs;
    std::atomic<int> jobs_in_progress = 0;
};

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


#endif /* ifndef UTILITIES_HPP */
