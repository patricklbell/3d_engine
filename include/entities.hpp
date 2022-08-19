#ifndef ENTITIES_HPP
#define ENTITIES_HPP
#include <cstddef>
#include <glm/gtc/quaternion.hpp>

#include "globals.hpp"
#include "assets.hpp"
#include "graphics.hpp"
#include <stack>

#define NULLID Id(-1, -1)

struct Id {
    int i;
    int v = 0;
    Id(int _i, int _v) : i(_i), v(_v){}
};

enum EntityType {
    ENTITY         = 0,
    MESH_ENTITY    = 1 << 0,
    WATER_ENTITY   = 1 << 1,
    TERRAIN_ENTITY = 1 << 2,
};

struct Entity {
    EntityType type = EntityType::ENTITY;
    Id id;
    Entity(Id _id=NULLID) : id(_id){
    }
};

struct MeshEntity : Entity {
    Mesh* mesh = nullptr;

    glm::vec3 position      = glm::vec3(0.0);
    glm::quat rotation      = glm::quat(0.0,0.0,0.0,1.0);
    glm::mat3 scale         = glm::mat3(1.0);
    bool casts_shadow = true;

    MeshEntity(Id _id=NULLID) : Entity(_id){
        type = (EntityType)(type | EntityType::MESH_ENTITY);
    }
};

struct WaterEntity : Entity {
    glm::vec3 position = glm::vec3(0.0);
    glm::mat3 scale    = glm::mat3(1.0);
    glm::vec4 shallow_color = glm::vec4(0.20, 0.7, 1.0, 1.0);
    glm::vec4 deep_color    = glm::vec4(0.08, 0.4, 0.8, 1.0);
    glm::vec4 foam_color    = glm::vec4(1.0,  1.0, 1.0, 1.0);

    WaterEntity(Id _id=NULLID) : Entity(_id){
        type = (EntityType)(type | EntityType::WATER_ENTITY);
    }
};

struct TerrainEntity : Entity {
    glm::vec3 position = glm::vec3(0.0);
    glm::mat3 scale    = glm::mat3(1.0);
    Texture *texture;

    TerrainEntity(Id _id=NULLID) : Entity(_id){
        type = (EntityType)(type | EntityType::TERRAIN_ENTITY);
    }
};

inline Entity *allocateEntity(Id id, EntityType type){
    if(type & WATER_ENTITY)
        return new WaterEntity(id);
    if(type & TERRAIN_ENTITY)
        return new TerrainEntity(id);
    if(type & MESH_ENTITY)
        return new MeshEntity(id);

    return new Entity(id);
}
inline constexpr size_t entitySize(EntityType type){
    switch (type) {
        case TERRAIN_ENTITY:
            return sizeof(TerrainEntity);
        case MESH_ENTITY:
            return sizeof(MeshEntity);
        case WATER_ENTITY:
            return sizeof(WaterEntity);
        default:
            return sizeof(Entity);
    }
}

struct EntityManager {
    Entity *entities[ENTITY_COUNT] = {nullptr};
    int versions[ENTITY_COUNT] = {0};
    std::stack<int> free_entity_stack;
    std::stack<int> delete_entity_stack;
    int id_counter = 0;

    WaterEntity *water = nullptr;

    ~EntityManager(){
        clear();
    }
    inline void clear(){
        // Delete entities
        for(int i = 0; i < ENTITY_COUNT; i++){
            if(entities[i] != nullptr) free (entities[i]);
            entities[i] = nullptr;
        }
        memset(versions, 0, sizeof(versions));
        free_entity_stack = {};
        delete_entity_stack = {};
        if(water != nullptr) {
            free(water);
            water = nullptr;
        }
        id_counter = 0;
    }
    inline Entity *getEntity(Id id){
        if(id.i < 0 || id.i > ENTITY_COUNT || id.v != versions[id.i]) return nullptr;
        return entities[id.i];
    }
    void setEntity(int index, Entity *e){
        entities[index] = e;
        // @overflow
        versions[index]++;
        e->id = Id(index, versions[index]);
    }
    inline void deleteEntity(Id id){
        delete_entity_stack.push(id.i);
    }
    inline Entity *duplicateEntity(Id id){
        auto src = getEntity(id);
        if(src == nullptr) return nullptr;

        auto cp = allocateEntity(NULLID, src->type);
        memcpy(cp, src, entitySize(src->type));
        
        setEntity(getFreeId().i, cp);
        return cp;
    }
    inline Id getFreeId(){
        int i;
        if(free_entity_stack.size() == 0){
            i = id_counter++;
            assert(id_counter < ENTITY_COUNT);
        } else {
            i = free_entity_stack.top();
            free_entity_stack.pop();
        }
        return Id(i, versions[i]);
    }
    inline void propogateChanges(){
        // Delete entities
        while(delete_entity_stack.size() != 0){
            int i = delete_entity_stack.top();
            delete_entity_stack.pop();
            free_entity_stack.push(i);
            if(entities[i] != nullptr) free (entities[i]);
            entities[i] = nullptr;
        }
    }
};

#endif
