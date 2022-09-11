#ifndef ENTITIES_HPP
#define ENTITIES_HPP
#include <cstddef>
#include <stack>

#include <glm/gtc/quaternion.hpp>

#include "globals.hpp"
#include "graphics.hpp"
#include "assets.hpp"

#define NULLID Id(-1, -1)

struct Id {
    uint64_t i = -1;
    uint16_t v = -1;

    constexpr bool operator!=(const Id& other) const {
        return (i != other.i) || (v != other.v);
    }
    constexpr bool operator==(const Id& other) const {
        return (i == other.i) && (v == other.v);
    }
    Id(int _i, int _v) : i(_i), v(_v){}
    Id() : i(-1), v(-1) {}
};

enum EntityType : uint64_t {
    ENTITY                  = 0,
    MESH_ENTITY             = 1 << 0,
    WATER_ENTITY            = 1 << 1,
    COLLIDER_ENTITY         =(1 << 2) | MESH_ENTITY,
    VEGETATION_ENTITY       = 1 << 3,
    ANIMATED_MESH_ENTITY    = 1 << 4,
};

struct Entity {
    EntityType type = ENTITY;
    Id id;
    Entity(Id _id=NULLID) : id(_id){
    }
};

struct MeshEntity : Entity {
    Mesh* mesh = nullptr;
    glm::vec3 albedo_mult = glm::vec3(1.0);
    float roughness_mult = 1.0;
    float metal_mult = 1.0;
    float ao_mult = 1.0;

    glm::vec3 position      = glm::vec3(0.0);
    glm::quat rotation      = glm::quat(0.0,0.0,0.0,1.0);
    glm::mat3 scale         = glm::mat3(1.0);
    bool casts_shadow = true;

    MeshEntity(Id _id=NULLID) : Entity(_id){
        type = EntityType::MESH_ENTITY;
    }
};


struct AnimatedMeshEntity : Entity {
    AnimatedMesh* animesh = nullptr;
    // Produced by traversing node tree and updated per tick
    std::array<glm::mat4, MAX_BONES> final_bone_matrices = { glm::mat4(1.0f) };

    float current_time = 0.0f;
    float time_scale = 1.0f;
    bool loop = false;
    bool playing = false;
    AnimatedMesh::Animation* animation = nullptr;

    glm::vec3 albedo_mult = glm::vec3(1.0);
    float roughness_mult = 1.0;
    float metal_mult = 1.0;
    float ao_mult = 1.0;

    glm::vec3 position = glm::vec3(0.0);
    glm::quat rotation = glm::quat(0.0, 0.0, 0.0, 1.0);
    glm::mat3 scale = glm::mat3(1.0);
    bool casts_shadow = true;

    AnimatedMeshEntity(Id _id = NULLID) : Entity(_id) {
        type = EntityType::ANIMATED_MESH_ENTITY;
    }

    bool tick(float dt);
    bool play(const std::string& name, float start_time, float _time_scale, bool _loop);
};

struct WaterEntity : Entity {
    glm::vec3 position = glm::vec3(0.0);
    glm::mat3 scale    = glm::mat3(1.0);
    glm::vec4 shallow_color = glm::vec4(0.20, 0.7, 1.0, 1.0);
    glm::vec4 deep_color    = glm::vec4(0.08, 0.4, 0.8, 1.0);
    glm::vec4 foam_color    = glm::vec4(1.0,  1.0, 1.0, 1.0);

    WaterEntity(Id _id=NULLID) : Entity(_id){
        type = EntityType::WATER_ENTITY;
    }
};

// For now axis aligned bounding box
struct ColliderEntity : MeshEntity {
    glm::vec3 collider_position  = glm::vec3(0.0);
    glm::quat collider_rotation  = glm::quat(0.0, 0.0, 0.0, 1.0);
    glm::mat3 collider_scale     = glm::mat3(1.0);
    bool selectable = false;

    ColliderEntity(Id _id = NULLID) : MeshEntity(_id) {
        type = EntityType::COLLIDER_ENTITY;
    }
};


struct VegetationEntity : Entity {
    Texture *texture = nullptr;

    glm::vec3 position      = glm::vec3(0.0);
    glm::quat rotation      = glm::quat(0.0,0.0,0.0,1.0);
    glm::mat3 scale         = glm::mat3(1.0);

    // @todo implement shadow casting
    bool casts_shadow = true;

    VegetationEntity(Id _id=NULLID) : Entity(_id){
        type = EntityType::VEGETATION_ENTITY;
    }
};



inline Entity *allocateEntity(Id id, EntityType type){
    switch (type) {
        case ANIMATED_MESH_ENTITY:
            return new AnimatedMeshEntity(id);
        case VEGETATION_ENTITY:
            return new VegetationEntity(id);
        case COLLIDER_ENTITY:
            return new ColliderEntity(id);
        case MESH_ENTITY:
            return new MeshEntity(id);
        case WATER_ENTITY:
            return new WaterEntity(id);
        default:
            return new Entity(id);
    }
}
inline constexpr size_t entitySize(EntityType type){
    switch (type) {
        case ANIMATED_MESH_ENTITY:
            return sizeof(AnimatedMeshEntity);
        case VEGETATION_ENTITY:
            return sizeof(VegetationEntity);
        case COLLIDER_ENTITY:
            return sizeof(ColliderEntity);
        case MESH_ENTITY:
            return sizeof(MeshEntity);
        case WATER_ENTITY:
            return sizeof(WaterEntity);
        default:
            return sizeof(Entity);
    }
}

inline Entity* copyEntity(Entity* src) {
    auto cpy = allocateEntity(src->id, src->type);
    memcpy(cpy, src, entitySize(src->type));
    return cpy;
}

struct EntityManager {
    Entity *entities[ENTITY_COUNT] = {nullptr};
    uint16_t versions[ENTITY_COUNT] = {0};
    std::stack<uint64_t> free_entity_stack;
    std::stack<uint64_t> delete_entity_stack;
    uint64_t id_counter = 0;

    Id water = NULLID;

    ~EntityManager(){
        clear();
    }
    inline void clear(){
        // Delete entities
        for(uint64_t i = 0; i < ENTITY_COUNT; i++){
            if(entities[i] != nullptr) free (entities[i]);
            entities[i] = nullptr;
        }
        memset(versions, 0, sizeof(versions));
        free_entity_stack = {};
        delete_entity_stack = {};
        water = NULLID;

        id_counter = 0;
    }
    inline Entity *getEntity(Id id) const {
        if(id.i < 0 || id.i > ENTITY_COUNT || id.v != versions[id.i]) return nullptr;
        return entities[id.i];
    }
    void setEntity(uint64_t index, Entity *e){
        entities[index] = e;
        // @note overflows
        versions[index]++;
        e->id = Id(index, versions[index]);
    }
    inline void deleteEntity(Id id){
        delete_entity_stack.push(id.i);
    }
    inline Entity *duplicateEntity(Id id) {
        auto src = getEntity(id);
        if(src == nullptr) return nullptr;

        auto cpy = copyEntity(src);
        cpy->id = getFreeId();
        
        setEntity(cpy->id.i, cpy);
        return cpy;
    }
    inline Id getFreeId(){
        uint64_t i;
        if(free_entity_stack.size() == 0){
            i = id_counter++;
            assert(id_counter < ENTITY_COUNT);
        } else {
            i = free_entity_stack.top();
            free_entity_stack.pop();
        }
        return Id(i, versions[i]);
    }
    inline void propogateChanges() {
        // Delete entities
        while(delete_entity_stack.size() != 0){
            int i = delete_entity_stack.top();
            delete_entity_stack.pop();
            free_entity_stack.push(i);
            if(entities[i] != nullptr) free (entities[i]);
            entities[i] = nullptr;
        }
    }
    inline Entity* createEntity(EntityType type) {
        auto id = getFreeId();
        auto e = allocateEntity(id, type);
        setEntity(id.i, e);
        return e;
    }

    void tickAnimatedMeshes(float dt) {
        for (int i = 0; i < ENTITY_COUNT; ++i) {
            auto e = reinterpret_cast<AnimatedMeshEntity*>(entities[i]);
            
            if (e != nullptr && e->type & EntityType::ANIMATED_MESH_ENTITY) {
                e->tick(dt);
                continue;
            }
        }
    }
};

#endif
