#ifndef ENTITIES_CORE_HPP
#define ENTITIES_CORE_HPP
#include <cstddef>
#include <stack>
#include <iostream>

#include <glm/gtc/quaternion.hpp>

#include "globals.hpp"
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
    Id(int _i, int _v) : i(_i), v(_v) {}
    Id() : i(-1), v(-1) {}
};

enum EntityType : uint64_t {
    ENTITY = 0,
    MESH_ENTITY = 1 << 0,
    WATER_ENTITY = 1 << 1,
    COLLIDER_ENTITY = (1 << 2) | MESH_ENTITY,
    // Empty slot
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

    glm::vec4 shallow_color = glm::vec4(0.20, 0.7, 1.0, 1.0);
    glm::vec4 deep_color = glm::vec4(0.08, 0.4, 0.8, 1.0);
    glm::vec4 foam_color = glm::vec4(1.0, 1.0, 1.0, 1.0);

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
        memcpy(versions, e1.versions, sizeof(versions[0]) * ENTITY_COUNT);
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
        memcpy(versions, e1.versions, sizeof(versions[0]) * ENTITY_COUNT);
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
            if (entities[i] != nullptr) free(entities[i]);
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
        if (id.i < 0 || id.i > ENTITY_COUNT || id.v != versions[id.i]) return nullptr;
        return entities[id.i];
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
            if (entities[i] != nullptr) free(entities[i]);
            entities[i] = nullptr;
        }
    }
    inline Entity* createEntity(EntityType type) {
        auto id = getFreeId();
        auto e = allocateEntity(id, type);
        setEntity(id.i, e);
        return e;
    }

    void tickAnimatedMeshes(float dt, bool paused) {
        for (int i = 0; i < ENTITY_COUNT; ++i) {
            auto e = reinterpret_cast<AnimatedMeshEntity*>(entities[i]);

            if (e != nullptr && entityInherits(e->type, EntityType::ANIMATED_MESH_ENTITY) && (!paused || e->draw_animated)) {
                e->tick(dt);
                continue;
            }
        }
    }
};

#endif // ENTITIES_CORE_HPP