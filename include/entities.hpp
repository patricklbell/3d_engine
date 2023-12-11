#ifndef ENGINE_ENTITIES_CORE_HPP
#define ENGINE_ENTITIES_CORE_HPP
#include <cstddef>
#include <stack>
#include <iostream>

#include <glm/gtc/quaternion.hpp>

#include <Jolt/Jolt.h>
#include "Jolt/Physics/Body/BodyCreationSettings.h"

#include "globals.hpp"
#include "assets.hpp"
#include "physics.hpp"

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
    Id(uint64_t _i, uint16_t _v) : i(_i), v(_v) {}
    Id(uint64_t phys_id) : i(phys_id >> 16), v(phys_id & 0xffff) {}
    Id() : i(-1), v(-1) {}

    constexpr uint64_t to_phys() {
        return (i << 16) | i;
    }
};

enum EntityType : uint64_t {
    ENTITY = 0,
    MESH_ENTITY = 1 << 0,
    WATER_ENTITY = 1 << 1,
    COLLIDER_ENTITY = (1 << 2) | MESH_ENTITY,
    POINT_LIGHT_ENTITY = 1 << 3,
    ANIMATED_MESH_ENTITY = (1 << 4) | MESH_ENTITY,
    PLAYER_ENTITY = (1 << 5) | ANIMATED_MESH_ENTITY,
};
struct Entity {
    EntityType type = ENTITY;
    Id id;
    Entity(Id _id = NULLID) : id(_id) {
    }
};

struct MeshEntity : Entity {
    JPH::BodyCreationSettings* body_settings = nullptr;
    JPH::BodyID body_id = JPH::BodyID();

    glm::vec3 position = glm::vec3(0.0);
    glm::quat rotation = glm::quat();
    glm::mat3 scale = glm::mat3(1.0);

    // @editor
    glm::vec3 gizmo_position_offset = glm::vec3(0.0);

    Mesh* mesh = nullptr;
    std::unordered_map<uint64_t, Material> overidden_materials; // submesh indices whose material we want to override

    uint8_t casts_shadow = true;
    uint8_t do_lightmap = true;

    MeshEntity(Id _id = NULLID) : Entity(_id) {
        type = EntityType::MESH_ENTITY;
    }
    ~MeshEntity() {
        if (body_settings != nullptr)
            delete body_settings;
    }
};

struct PointLightEntity : Entity {
    glm::vec3 position = glm::vec3(0.0);
    glm::vec3 radiance = glm::vec3(1.0);
    float radius = 1.0;

    PointLightEntity(Id _id = NULLID) : Entity(_id) {
        type = EntityType::POINT_LIGHT_ENTITY;
    }
};

struct AnimatedMeshEntity : MeshEntity {
    // Asset which contains all the bones and animations
    AnimatedMesh* animesh = nullptr;

    // Produced by traversing node tree and updated per tick
    std::array<glm::mat4, MAX_BONES> final_bone_matrices = { glm::mat4(1.0f) };

    // @editor Used by editor to toggle drawing animation, ignored when playing
    bool draw_animated = false;

    // Global multiplier for all animations, here for convenience
    float time_scale_mult = 1.0;

    // 
    // Animation state machine
    //
    // If there are multiple events we need to keep the previous one buffered for
    // blending, this flag determines wheter event 0 is playing
    bool playing_first = true;

    // We need to apply any transforms at the start of the next animation,
    // @todo make this better/extensible for when transforms are applied continuously/keyframes
    bool apply_animation_transform = false;
    glm::vec3 animation_delta_position = glm::vec3(0.0);
    glm::quat animation_delta_rotation = glm::quat();

    //@debug
    enum class BlendState {
        NONE = 0,
        PREVIOUS,
        NEXT,
    } blend_state = BlendState::NONE;
    struct AnimationEvent {
        bool playing = true;
        bool loop = false;

        float start_time = 0.0f;
        float current_time = 0.0f;
        float duration = 1.0f;
        float time_scale = 1.0f;

        AnimatedMesh::Animation* animation = nullptr;

        // Blending with previous and next animation/default state
        bool blend = false;
        float blend_prev_start = 0.0;
        float blend_prev_end = 0.1;
        float blend_prev_amount = 0.5;
        float blend_next_start = 0.9;
        float blend_next_amount = 0.5;

        // Transforms to apply during animation
        glm::vec3 delta_position = glm::vec3(0.0);
        glm::quat delta_rotation = glm::quat();
        glm::mat4 delta_transform = glm::mat4(1.0f); // This must be updated or transform won't properly be blended

        // Whether to apply these transforms to entity when animation finishes
        bool transform_entity = false;
        bool transform_inverted = false; // Used for blending with previous
    };
    std::vector<AnimationEvent> animation_events;
    AnimationEvent default_event;

    AnimatedMeshEntity(Id _id = NULLID) : MeshEntity(_id) {
        type = EntityType::ANIMATED_MESH_ENTITY;
    }

    bool tick(float dt);
    void init();
    AnimationEvent* play(const std::string& name, float start_time = 0.0f, bool fallback = false, bool immediate = false, bool playing = true);
    bool isAnimationFinished();
    bool isDefaultAnimationFinished();
};


struct WaterEntity : Entity {
    glm::vec3 position = glm::vec3(0.0);
    glm::mat3 scale = glm::mat3(1.0);

    //glm::vec4 ssr_settings{ 0.5, 20.0, 10.0, 20.0 };
    //float depth_fade_distance{ 0.5f };

    glm::vec3 refraction_tint_col{ 0.003f, 0.599f, 0.812f };
    glm::vec3 surface_col{ 0.465, 0.797, 0.991 };
    glm::vec3 floor_col{ 0.165, 0.397, 0.491 };
    glm::vec4 normal_scroll_direction{ 1.0f, 0.0f, 0.0f, 1.0f };
    glm::vec2 normal_scroll_speed{ 0.01f, 0.01f };
    glm::vec2 tilling_size{ 5.0f, 5.0f };
    float refraction_distortion_factor{ 0.04f };
    float refraction_height_factor{ 2.5f };
    float refraction_distance_factor{ 15.0f };
    float foam_height_start{ 0.8f };
    float foam_angle_exponent{ 80.0f };
    float foam_tilling{ 2.0f };
    float foam_brightness{ 0.8f };
    float roughness{ 0.08f };
    float reflectance{ 0.55f };
    float specular_intensity{ 125.0f };
    float floor_height{ 5.0 };
    float peak_height{ 6.0 };
    float extinction_coefficient{ 0.4 };

    WaterEntity(Id _id = NULLID) : Entity(_id) {
        type = EntityType::WATER_ENTITY;
    }
};

// For now axis aligned bounding box
struct ColliderEntity : MeshEntity {
    glm::vec3 collider_position = glm::vec3(0.0);
    glm::quat collider_rotation = glm::quat(0.0, 0.0, 0.0, 1.0);
    glm::mat3 collider_scale = glm::mat3(0.5);

    uint8_t selectable = false;

    ColliderEntity(Id _id = NULLID) : MeshEntity(_id) {
        type = EntityType::COLLIDER_ENTITY;
        scale = glm::mat3(0.5);
    }
};

enum class PlayerActionType : uint64_t {
    NONE = 0,
    STEP_FORWARD,
    TURN_LEFT,
    TURN_RIGHT,
};

struct PlayerAction {
    PlayerActionType type = PlayerActionType::NONE;

    glm::fvec3 delta_position = glm::vec3(0.0);
    glm::fquat delta_rotation = glm::quat();

    bool active = false;
    float duration = 0.0f;
    float time = 0.0f;
};

const int MAX_ACTION_BUFFER = 3;
const float MAX_ACTION_SPEEDUP = 1.4f;
struct PlayerEntity : AnimatedMeshEntity {
    std::vector<PlayerAction> actions;

    PlayerEntity(Id _id = NULLID) : AnimatedMeshEntity(_id) {
        type = EntityType::PLAYER_ENTITY;
    }

    bool turn_left();
    bool turn_right();
    bool step_forward();
};

inline Entity* allocateEntity(Id id, EntityType type) {
    switch (type) {
    case MESH_ENTITY:
        return new MeshEntity(id);
    case WATER_ENTITY:
        return new WaterEntity(id);
    case COLLIDER_ENTITY:
        return new ColliderEntity(id);
    case ANIMATED_MESH_ENTITY:
        return new AnimatedMeshEntity(id);
    case PLAYER_ENTITY:
        return new PlayerEntity(id);
    case POINT_LIGHT_ENTITY:
        return new PointLightEntity(id);

    default:
        return new Entity(id);
    }
}
inline constexpr size_t entitySize(EntityType type) {
    switch (type) {
    case ANIMATED_MESH_ENTITY:
        return sizeof(AnimatedMeshEntity);
    case COLLIDER_ENTITY:
        return sizeof(ColliderEntity);
    case MESH_ENTITY:
        return sizeof(MeshEntity);
    case WATER_ENTITY:
        return sizeof(WaterEntity);
    case PLAYER_ENTITY:
        return sizeof(PlayerEntity);
    case POINT_LIGHT_ENTITY:
        return sizeof(PointLightEntity);

    default:
        return sizeof(Entity);
    }
}

inline constexpr bool entityInherits(EntityType derived, EntityType base) {
    return (derived & base) == base;
}

inline Entity* copyEntity(Entity* src) {
    auto cpy = allocateEntity(src->id, src->type);
    switch (src->type) {
    case ANIMATED_MESH_ENTITY:
        *((AnimatedMeshEntity*)cpy) = *((AnimatedMeshEntity*)src);
        break;
    case COLLIDER_ENTITY:
        *((ColliderEntity*)cpy) = *((ColliderEntity*)src);
        break;
    case MESH_ENTITY:
        *((MeshEntity*)cpy) = *((MeshEntity*)src);
        break;
    case WATER_ENTITY:
        *((WaterEntity*)cpy) = *((WaterEntity*)src);
        break;
    case PLAYER_ENTITY:
        *((PlayerEntity*)cpy) = *((PlayerEntity*)src);
        break;
    case POINT_LIGHT_ENTITY:
        *((PointLightEntity*)cpy) = *((PointLightEntity*)src);
        break;
    default:
        std::cerr << "Critical error, unknown entity type " << src->type << " in copyEntity!\n";
        assert(false);
    }
    return cpy;
}

struct EntityManager {
    Entity* entities[ENTITY_COUNT] = { nullptr };
    uint16_t versions[ENTITY_COUNT] = { 0 };
    std::stack<uint64_t> free_entity_stack;
    std::stack<uint64_t> delete_entity_stack;
    uint64_t id_counter = 0;

    Id water = NULLID;
    Id player = NULLID;

    ~EntityManager() { clear(); }
    EntityManager() = default;
    EntityManager(const EntityManager& e1) { // Copy constructor
        for (uint64_t i = 0; i < ENTITY_COUNT; i++) {
            if (e1.entities[i] != nullptr)
                entities[i] = copyEntity(e1.entities[i]);
        }
        std::memcpy(versions, e1.versions, sizeof(versions[0]) * ENTITY_COUNT);
        free_entity_stack = e1.free_entity_stack;
        delete_entity_stack = e1.delete_entity_stack;
        id_counter = e1.id_counter;
        water = e1.water;
        player = e1.player;
    }
    EntityManager& operator=(const EntityManager& e1) {
        for (uint64_t i = 0; i < ENTITY_COUNT; i++) {
            if (e1.entities[i] != nullptr)
                entities[i] = copyEntity(e1.entities[i]);
        }
        std::memcpy(versions, e1.versions, sizeof(versions[0]) * ENTITY_COUNT);
        free_entity_stack = e1.free_entity_stack;
        delete_entity_stack = e1.delete_entity_stack;
        id_counter = e1.id_counter;
        water = e1.water;
        player = e1.player;
        return *this;
    }

    inline void clear() {
        // Delete entities
        for (uint64_t i = 0; i < ENTITY_COUNT; i++) {
            if (entities[i] != nullptr) delete entities[i];
            entities[i] = nullptr;
        }
        memset(versions, 0, sizeof(versions));
        free_entity_stack = {};
        delete_entity_stack = {};
        water = NULLID;
        player = NULLID;

        id_counter = 0;
    }
    inline Entity* getEntity(Id id) const {
        if (id.i >= 0 && id.i < ENTITY_COUNT && id.v == versions[id.i])
            return entities[id.i];
        return nullptr;
    }
    void setEntity(uint64_t index, Entity* e) {
        entities[index] = e;
        // @note overflows
        versions[index]++;
        e->id = Id(index, versions[index]);
    }
    inline void deleteEntity(Id id) {
        delete_entity_stack.push(id.i);
    }
    inline Entity* duplicateEntity(Id id) {
        auto src = getEntity(id);
        if (src == nullptr) return nullptr;

        auto cpy = copyEntity(src);
        cpy->id = getFreeId();

        setEntity(cpy->id.i, cpy);
        return cpy;
    }
    inline Id getFreeId() {
        uint64_t i;
        if (free_entity_stack.size() == 0) {
            i = id_counter++;
            assert(id_counter < ENTITY_COUNT);
        }
        else {
            i = free_entity_stack.top();
            free_entity_stack.pop();
        }
        return Id(i, versions[i]);
    }
    inline void propogateChanges() {
        // Delete entities
        while (delete_entity_stack.size() != 0) {
            int i = delete_entity_stack.top();
            delete_entity_stack.pop();
            free_entity_stack.push(i);
            if (entities[i] != nullptr) delete entities[i];
            entities[i] = nullptr;
        }
    }
    inline Entity* createEntity(EntityType type) {
        auto id = getFreeId();
        auto e = allocateEntity(id, type);
        setEntity(id.i, e);
        return e;
    }
};

void tickEntities(EntityManager& entities, float dt, bool is_playing);

#endif // ENGINE_ENTITIES_CORE_HPP
