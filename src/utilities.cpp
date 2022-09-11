#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <limits>
#include <thread>
#include <vector>
#include <cstdint>
#include <set>
#include <unordered_map>
#include <iostream>
#include "assets.hpp"
#include "entities.hpp"
#include "glm/detail/type_mat.hpp"
#include "glm/detail/type_vec.hpp"
#include "glm/fwd.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "globals.hpp"
#include "graphics.hpp"
#include "utilities.hpp"
#include "texture.hpp"

template<typename T, glm::precision P>
std::ostream &operator<<(std::ostream &os, const glm::tvec1<T, P> &v) {
    return os << v.x;
}

template<typename T, glm::precision P>
std::ostream &operator<<(std::ostream &os, const glm::tvec2<T, P> &v) {
    return os << v.x << ", " << v.y;
}

template<typename T, glm::precision P>
std::ostream &operator<<(std::ostream &os, const glm::tvec3<T, P> &v) {
    return os << v.x << ", " << v.y << ", " << v.z;
}

template<typename T, glm::precision P>
std::ostream &operator<<(std::ostream &os, const glm::tvec4<T, P> &v) {
    return os << v.x << ", " << v.y << ", " << v.z << ", " << v.w;
}

float linearstep(float e1, float e2, float x) {
    return (x - e1) / (e2 - e1);
}

std::vector<std::string> split(std::string s, std::string delimiter) {
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    std::string token;
    std::vector<std::string> res;

    while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
        token = s.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back(token);
    }

    res.push_back(s.substr(pos_start));
    return res;
}

// Based on:
// https://www.codeproject.com/Tips/1263121/Copy-a-GL-Texture-to-Another-GL-Texture-or-to-a-GL
/// Check and report details for known GL error types.
/// </summary>
/// <param name="className">The class wherein the error occurs.</param>
/// <param name="methodName">The method name wherein the error occurs.</param>
/// <param name="callName">The call that causes the error.</param>
/// <remarks>
/// With .NET 4.5 it is possible to use equivalents for C/C++ <c>__FILE__</c> and <c>__LINE__</c>.
/// On older .NET versions we use <c>className</c>, <c>methodName</c> and <c>callName</c> instead.
/// </remarks>
void checkGLError(std::string identifier)
{
    int error_code = (int)glGetError();
    if(error_code == 0)
        return;

    std::string error = "Unknown error";
    std::string description = "No description";

    if (error_code == GL_INVALID_ENUM)
    {
        error = "GL_INVALID_ENUM";
        description = "An unacceptable value has been specified for an enumerated argument.";
    }
    else if (error_code == GL_INVALID_VALUE)
    {
        error = "GL_INVALID_VALUE";
        description = "A numeric argument is out of range.";
    }
    else if (error_code == GL_INVALID_OPERATION)
    {
        error = "GL_INVALID_OPERATION";
        description = "The specified operation is not allowed in the current state.";
    }
    else if (error_code == GL_STACK_OVERFLOW)
    {
        error = "GL_STACK_OVERFLOW";
        description = "This command would cause a stack overflow.";
    }
    else if (error_code == GL_STACK_UNDERFLOW)
    {
        error = "GL_STACK_UNDERFLOW";
        description = "This command would cause a stack underflow.";
    }
    else if (error_code == GL_OUT_OF_MEMORY)
    {
        error = "GL_OUT_OF_MEMORY";
        description = "There is not enough memory left to execute the command.";
    }
    else if (error_code == GL_INVALID_FRAMEBUFFER_OPERATION)
    {
        error = "GL_INVALID_FRAMEBUFFER_OPERATION";
        description = "The object bound to FRAMEBUFFER_BINDING is not 'framebuffer complete'.";
    }
    else if (error_code == GL_CONTEXT_LOST)
    {
        error = "GL_CONTEXT_LOST";
        description = "The context has been lost, due to a graphics card reset.";
    }
    else if (error_code == GL_TABLE_TOO_LARGE)
    {
        error = "GL_TABLE_TOO_LARGE";
        description = std::string("The exceeds the size limit. This is part of the ") +
                      "(Architecture Review Board) ARB_imaging extension.";
    }

    std::cerr << "An internal OpenGL call failed in at '" + identifier + "' " +
                       "Error '" + error + "' description: " + description + "\n";
}

void saveLevel(EntityManager & entity_manager, const std::string & level_path, const Camera &camera){
    std::cout << "----------- Saving Level " << level_path << "----------\n";

    // @todo handle nullptr on mesh
    std::set<Mesh*> used_meshes;
    for(int i = 0; i < ENTITY_COUNT; i++){
        auto e = entity_manager.entities[i];
        if(e == nullptr || !(e->type & EntityType::MESH_ENTITY)) continue;
        auto m_e = reinterpret_cast<MeshEntity*>(e);

        used_meshes.emplace(m_e->mesh);
    }

    FILE *f;
    f=fopen(level_path.c_str(), "wb");

    // @note uses extra memory because writes matrices
    fwrite(&camera, sizeof(camera), 1, f);

    // Construct map between mesh pointers and index
    std::unordered_map<intptr_t, uint64_t> asset_lookup;
    uint64_t index = 0;
    for(const auto &mesh : used_meshes){
        asset_lookup[(intptr_t)mesh] = index;
        fwrite(mesh->handle.data(), mesh->handle.size(), 1, f);

        // std strings are not null terminated so add it
        fputc('\0', f);
        ++index;
    }

    // Write extra null terminator to signal end of asset paths
    fputc('\0', f);

    for(int i = 0; i < ENTITY_COUNT; i++){
        auto e = entity_manager.entities[i];
        if(e == nullptr || e->type == ENTITY) continue;

        fwrite(&e->type, sizeof(EntityType), 1, f);

        // Handle resource pointers with path lut
        if(e->type & MESH_ENTITY) {
            auto m_e = (MeshEntity*)e;

            // Hacky way of writing pointers by mapping to indexes into a list
            auto mesh = m_e->mesh;
            
            auto lookup = asset_lookup[reinterpret_cast<intptr_t>(mesh)];
            m_e->mesh = reinterpret_cast<Mesh*>(lookup);

            fwrite(e, entitySize(e->type), 1, f);
                   
            // Restore real pointer
            m_e->mesh = mesh;
        }
        // Write entities which contain no pointers
        else {
            fwrite(e, entitySize(e->type), 1, f);
        }
    }

    fclose(f);
}

// Overwrites existing entity indices
// @todo if needed make loading asign new ids such that connections are maintained
// @todo any entities that store ids must be resolved so that invalid ie wrong versions are NULLID 
// since we dont maintain version either
bool loadLevel(EntityManager &entity_manager, AssetManager &asset_manager, const std::string &level_path, Camera &camera) {
    std::cout << "---------- Loading Level " << level_path << "----------\n";

    FILE *f;
    f=fopen(level_path.c_str(),"rb");

    if (!f) {
        // @debug
        std::cerr << "Error in reading level file at path: " << level_path << ".\n";
        return false;
    }

    entity_manager.clear();

    fread(&camera, sizeof(camera), 1, f);

    std::string mesh_path;
    uint64_t mesh_index = 0;
    std::unordered_map<uint64_t, Mesh*> index_to_mesh;

    char c;
    while((c = fgetc(f)) != EOF){
        if(c == '\0'){
            // Reached end of paths
            if(mesh_path == "") break;

            auto mesh = asset_manager.getMesh(mesh_path);
            // Asset not loaded already so load from file 
            if(mesh == nullptr){
                std::cout << "Loading new mesh at path " << mesh_path << ".\n";

                // In future this should probably be true for all meshes
                mesh = asset_manager.createMesh(mesh_path);
                if (endsWith(mesh_path, ".mesh")) {
                    asset_manager.loadMeshFile(mesh, mesh_path);
                }
                else {
                    std::cerr << "Warning, new mesh is being loaded with assimp\n";
                    asset_manager.loadMeshAssimp(mesh, mesh_path);
                }

                index_to_mesh[mesh_index] = mesh;
            } else {
                std::cout << "Using existing asset at path " << mesh_path << ".\n";
                index_to_mesh[mesh_index] = mesh;
            }

            mesh_path = "";
            mesh_index++;
        } else {
            mesh_path += c;
        }
    }

    while((c = fgetc(f)) != EOF){
        ungetc(c, f);
        // If needed we can write id as well to maintain during saves
        EntityType type;
        fread(&type, sizeof(EntityType), 1, f);

        std::cout << "Reading entity of type " << type << ":\n";
        if(type & WATER_ENTITY)
        {
            auto e = (WaterEntity*)allocateEntity(NULLID, type);
            fread(e, entitySize(type), 1, f);
            if(entity_manager.water == NULLID) {
                entity_manager.setEntity(entity_manager.getFreeId().i, e);
                entity_manager.water = e->id;
            } else {
                free(e);
                std::cerr << "Duplicate water in level, skipping\n";
            }
        }
        // Handle resource pointers with path lut
        else if(type & MESH_ENTITY)
        {
            auto e = allocateEntity(NULLID, type);
            fread(e, entitySize(type), 1, f);
            entity_manager.setEntity(entity_manager.getFreeId().i, e);

            auto m_e = (MeshEntity*)e;
            
            // Convert asset index to ptr
            auto file_asset_index = reinterpret_cast<uint64_t>(m_e->mesh);
            m_e->mesh = index_to_mesh[file_asset_index];

            std::cout << "\tAsset index is " << file_asset_index << ".\n";
            std::cout << "\tPosition is " << m_e->position << ".\n";
        }
        else
        {
            auto e = allocateEntity(NULLID, type);
            fread(e, entitySize(type), 1, f);
            entity_manager.setEntity(entity_manager.getFreeId().i, e);
        }
    }
    fclose(f);

    // Update water collision map
    if (entity_manager.water != NULLID) {
        // @todo make better systems for determining when to update shadow map
        auto water = (WaterEntity*)entity_manager.getEntity(entity_manager.water);
        if (water != nullptr) {
            bindDrawWaterColliderMap(entity_manager, water);
            blurWaterFbo(water);
        }
        else {
            entity_manager.water = NULLID;
        }
    }

    return true;
}

// @perf These could probably be made more efficient
glm::mat4x4 createModelMatrix(const glm::vec3 &pos, const glm::quat &rot, const glm::mat3x3 &scl){
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

// https://cadxfem.org/inf/Fast%20MinimumStorage%20RayTriangle%20Intersection.pdf
// Cornell university paper describing ray intersection algorithms
static const double epsilon = 0.000001;
bool rayIntersectsTriangleTest(const glm::vec3 vertices[3], const glm::vec3 &ray_origin, const glm::vec3 &ray_direction){
    glm::vec3 edge1 = vertices[1] - vertices[0];
    glm::vec3 edge2 = vertices[2] - vertices[0];
    glm::vec3 p = glm::cross(ray_direction, edge2);
    float det = glm::dot(edge1, p);
    if(det < epsilon && det > -epsilon) return false;

    float inv_det = 1.0f / det;
    
    auto t = ray_origin - vertices[0];
    float u = glm::dot(t, p) * inv_det;
    if(u < 0.0 || u > 1.0) return false;

    auto q = glm::cross(t, edge1);
    float v = glm::dot(ray_direction, q) * inv_det;
    if(v < 0.0 || u + v > 1.0) return false;

    return true;
}

bool rayIntersectsTriangleTestCull(const glm::vec3 vertices[3], const glm::vec3 &ray_origin, const glm::vec3 &ray_direction){
    auto edge1 = vertices[1] - vertices[0];
    auto edge2 = vertices[2] - vertices[0];
    auto p = glm::cross(ray_direction, edge2);
    float det = glm::dot(edge1, p);
    if(det < epsilon) return false;

    auto t = ray_origin - vertices[0];
    float u = glm::dot(t, p);
    if(u < 0.0 || u > det) return false;

    auto q = glm::cross(t, edge1);
    float v = glm::dot(ray_direction, q);
    if(v < 0.0 || u + v > det) return false;

    return true;
}

// Returns whether ray intersects and calculates t, u, v where t is the distance 
// from ray origin to the intersection, u and v are the barycentric coordinate
// of the intersection. Tests both sides of face i.e. direction doesn't matter
bool rayIntersectsTriangle(const glm::vec3 vertices[3], const glm::vec3 &ray_origin, const glm::vec3 &ray_direction, double &t, double &u, double &v){
    glm::vec3 edge1 = vertices[1] - vertices[0];
    glm::vec3 edge2 = vertices[2] - vertices[0];
    glm::vec3 p = glm::cross(ray_direction, edge2);
    double det = glm::dot(edge1, p);
    if(det < epsilon && det > -epsilon) return false;

    double inv_det = 1.0f / det;
    
    auto t_vec = ray_origin - vertices[0];
    u = glm::dot(t_vec, p) * inv_det;
    if(u < 0.0 || u > 1.0) return false;

    auto q = glm::cross(t_vec, edge1);
    v = glm::dot(ray_direction, q) * inv_det;
    if(v < 0.0 || u + v > 1.0) return false;

    t = glm::dot(edge2, q) * inv_det;
    return true;
}

bool rayIntersectsMesh(Mesh *mesh, const glm::mat4x4 &transform, const Camera &camera, const glm::vec3 &ray_origin, const glm::vec3 &ray_direction, glm::vec3 &collision_point, glm::vec3 &normal){
    bool flag = false;
    float min_collision_distance;
    for(int j = 0; j < mesh->num_indices; j+=3){
        const auto p1 = mesh->vertices[mesh->indices[j]];
        const auto p2 = mesh->vertices[mesh->indices[j+1]];
        const auto p3 = mesh->vertices[mesh->indices[j+2]];
        glm::vec3 triangle[3] = {
            glm::vec3(transform * glm::vec4(p1, 1.0)),
            glm::vec3(transform * glm::vec4(p2, 1.0)),
            glm::vec3(transform * glm::vec4(p3, 1.0))
        };
        double u, v, t;
        if(rayIntersectsTriangle(triangle, ray_origin, ray_direction, t, u, v)){
            auto collision_distance = glm::length((ray_origin + ray_direction*(float)t) - camera.position);
            if(!flag || (collision_distance < min_collision_distance)){
                flag = true;
                min_collision_distance = collision_distance;
                collision_point = ray_origin + ray_direction*(float)t;
                // Use uv coordinates and all 3 normals
                normal = mesh->normals[mesh->indices[j]];
            }
        }
    }
    return flag;
}

// Returns whether ray intersects and calculates t, u, v where t is the distance 
// from ray origin to the intersection, u and v are the barycentric coordinate. Culls
// back face so ray must enter front of triangle ?? where CCW winding order is front
bool rayIntersectsTriangleCull(const glm::vec3 vertices[3], const glm::vec3 &ray_origin, const glm::vec3 &ray_direction, double &t, double &u, double &v){
    auto edge1 = vertices[1] - vertices[0];
    auto edge2 = vertices[2] - vertices[0];
    auto p = glm::cross(ray_direction, edge2);
    float det = glm::dot(edge1, p);
    if(det < epsilon) return false;

    auto t_vec = ray_origin - vertices[0];
    u = glm::dot(t_vec, p);
    if(u < 0.0 || u > det) return false;

    auto q = glm::cross(t_vec, edge1);
    v = glm::dot(ray_direction, q);
    if(v < 0.0 || u + v > det) return false;

    t = glm::dot(edge2, q);
    float inv_det = 1.0f / det;
    t *= inv_det;
    u *= inv_det;
    v *= inv_det;
    return true;
}
bool lineIntersectsPlane(const glm::vec3 &plane_origin, const glm::vec3 &plane_normal, const glm::vec3 &line_origin, const glm::vec3 &line_direction, float &t){
    float pn_ld = glm::dot(plane_normal, line_direction);
    // Line parallel to plane
    if(pn_ld < epsilon && pn_ld > -epsilon) return false;

    // p = lo + t*ld
    // pn dot (p - po) = 0
    // pn dot (lo + t*ld - po) = 0
    // t*(pn | ld) = pn | (po - lo)
    // t = pn | (po - lo) / (pn | ld)
    t = glm::dot(plane_normal, plane_origin - line_origin) / (pn_ld);

    return true;
}

bool lineIntersectsCube(const glm::vec3& cube_position, const glm::vec3& cube_scale, const glm::vec3& line_origin, const glm::vec3& line_direction, float& t, glm::vec3 &normal) {
    bool did_intersect = false;
    for (int i = 0; i < 3; i++) {
        for (int d = -1; d <= 1; d += 2) {
            auto p_normal = glm::vec3(d * (i == 0), d * (i == 1), d * (i == 2));
            auto p_position = cube_position + cube_scale * p_normal;
            
            float p_t;
            if (lineIntersectsPlane(p_position, p_normal, line_origin, line_direction, p_t)) {
                auto p = line_origin + p_t * line_direction;
                auto offset = glm::abs(p - p_position);
                auto p_tangent = cube_scale * glm::vec3((i == 1 || i == 2), (i == 0 || i == 2), (i == 0 || i == 1));
                offset *= p_tangent;

                // Test if points lies on bounded plane
                if (offset.x > p_tangent.x || offset.y > p_tangent.y || offset.z > p_tangent.z) {
                    continue;
                }
                
                if (!did_intersect || p_t < t) {
                    t = p_t;
                    normal = p_normal;
                    did_intersect = true;
                }
            }
        }
    }
    return did_intersect;
}

float closestDistanceBetweenLineCircle(const glm::vec3 &line_origin, const glm::vec3 &line_direction, const glm::vec3 &circle_center, const glm::vec3 &circle_normal, float circle_radius, glm::vec3& point)
{
    float t;
    // Check if line intersects circle plane, ie not parallel
    if(lineIntersectsPlane(circle_center, circle_normal, line_origin, line_direction, t))
    {
        // get the ray's intersection point on the plane which
        // contains the circle
        const glm::vec3 on_plane = line_origin + t*line_direction;
        // project that point on to the circle's circumference
        point = circle_center + circle_radius * normalize(on_plane - circle_center);
        return glm::length(on_plane - point);
    } else {
        // As line is parallel to circle this doesnt need to be projected?
        point = circle_radius * normalize(-line_direction);

        return glm::length(line_origin - point);
    }
}
float closestDistanceBetweenLines(const glm::vec3 &l1_origin, const glm::vec3 &l1_direction, const glm::vec3 &l2_origin, const glm::vec3 &l2_direction, float &l1_t, float &l2_t)
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

void screenPosToWorldRay(
    glm::ivec2 mouse_position,
    glm::mat4 view,                     // Camera position and orientation
    glm::mat4 projection,               // Camera parameters (ratio, field of view, near and far planes)
    glm::vec3& out_origin,              // Ouput : Origin of the ray. Starts at the near plane, so if you want the ray to start at the camera's position instead, ignore this.
    glm::vec3& out_direction            // Ouput : Direction, in world space, of the ray that goes "through" the mouse.
) {

    // The ray Start and End positions, in Normalized Device Coordinates
    glm::vec4 ray_start_NDC(
        ((float)mouse_position.x / (float)window_width - 0.5f) * 2.0f, // [0,1024] -> [-1,1]
        -((float)mouse_position.y / (float)window_height - 0.5f) * 2.0f, // [0, 768] -> [-1,1]
        -1.0, // The near plane maps to Z=-1 in Normalized Device Coordinates
        1.0f
    );
    glm::vec4 ray_end_NDC(
        ((float)mouse_position.x / (float)window_width - 0.5f) * 2.0f,
        -((float)mouse_position.y / (float)window_height - 0.5f) * 2.0f,
        0.0,
        1.0f
    );
    // Inverse of projection and view to obtain NDC to world
    glm::mat4 vp = glm::inverse(projection * view);
    glm::vec4 ray_start_world = vp * ray_start_NDC;
    ray_start_world/=ray_start_world.w;
    glm::vec4 ray_end_world   = vp * ray_end_NDC;
    ray_end_world  /=ray_end_world.w;


    out_direction = glm::vec3(glm::normalize(ray_end_world - ray_start_world));
    out_origin = glm::vec3(ray_start_world);
}

bool endsWith(std::string_view str, std::string_view suffix)
{
    return str.size() >= suffix.size() && 0 == str.compare(str.size()-suffix.size(), suffix.size(), suffix);
}

bool startsWith(std::string_view str, std::string_view prefix)
{
    return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
}

#include <filesystem>
#ifdef _WINDOWS
#include <windows.h>
std::string getexepath() {
    char result[MAX_PATH];
    auto p = std::filesystem::path(std::string(result, GetModuleFileName(NULL, result, MAX_PATH)));
    return p.parent_path().string();
}
#else
#include <limits.h>
#include <unistd.h>
std::string getexepath() {
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    auto p = std::filesystem::path(std::string(result, (count > 0) ? count : 0));
    return p.parent_path().string();
}
#endif

// ThreadPool
#include <thread>
#include <functional>

void ThreadPool::start() {
    const uint32_t num_threads = std::max((int)std::thread::hardware_concurrency() - 2, 4);
    threads.resize(num_threads);
    for (uint32_t i = 0; i < num_threads; i++) {
        // @note This lambda may be unnecessary
        threads.at(i) = std::thread(&ThreadPool::threadLoop, this);
    }
}

void ThreadPool::threadLoop() {
    while (true) {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            mutex_condition.wait(lock, [this] {
                return !jobs.empty() || should_terminate;
            });
            if (should_terminate) {
                return;
            }
            job = jobs.front();
            jobs_in_progress++;
            jobs.pop();
        }
        job();
        jobs_in_progress--;
    }
}

void ThreadPool::queueJob(const std::function<void()>& job) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        jobs.push(job);
    }
    mutex_condition.notify_one();
}

bool ThreadPool::busy() {
    std::unique_lock<std::mutex> lock(queue_mutex);
    return !(jobs.empty() && jobs_in_progress == 0);
}

void ThreadPool::stop() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        should_terminate = true;
    }
    mutex_condition.notify_all();
    for (std::thread& active_thread : threads) {
        active_thread.join();
    }
    threads.clear();
}
