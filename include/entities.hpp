#ifndef ENTITIES_HPP
#define ENTITIES_HPP
#include <glm/gtc/quaternion.hpp>

#include "globals.hpp"
#include "assets.hpp"
#include <stack>

enum EntityType {
    ENTITY = 0,
    MESH_ENTITY = 1,
};

struct Entity {
    EntityType type = EntityType::ENTITY;
    unsigned int id;
    Entity(int _id) : id(_id){
    }
};

struct MeshEntity : Entity {
    Mesh* mesh = nullptr;
    glm::vec3 position = glm::vec3(0.0);
    glm::quat rotation = glm::quat(0.0,0.0,0.0,1.0);
    glm::mat3 scale    = glm::mat3(1.0);
    bool casts_shadow = true;

    MeshEntity(int _id) : Entity(_id){
        type = (EntityType)(type | EntityType::MESH_ENTITY);
    }
};

struct EntityManager {
    Entity *entities[ENTITY_COUNT] = {nullptr};
    std::stack<int> free_entity_stack;
    std::stack<int> delete_entity_stack;
    int id_counter = 0;
   
    ~EntityManager(){
        for(int i = 0; i < ENTITY_COUNT; ++i){
            deleteEntity(i);
        }
        propogateChanges();
    }
    inline Entity *getEntity(int id){
        return entities[id];
    }
    inline void deleteEntity(int id){
        delete_entity_stack.push(selected_entity);
    }
    inline int getFreeId(){
        int id;
        if(free_entity_stack.size() == 0){
            id = id_counter++;
        } else {
            id = free_entity_stack.top();
            free_entity_stack.pop();
        }
        return id;
    }
    inline void propogateChanges(){
        // Delete entities
        while(delete_entity_stack.size() != 0){
            int id = delete_entity_stack.top();
            delete_entity_stack.pop();
            free_entity_stack.push(id);
            if(entities[id] != nullptr) free (entities[id]);
            entities[id] = nullptr;
        }
    }
};

//
//  Potentially more advanced implementation of entity storage, might just be a
//  waste of time
//
//struct Id {
//    unsigned int index;
//    unsigned int version = 0;
//} typedef Id;
//struct EntitySlotMap {
//    Entity entities[ENTITY_COUNT];
//    std::stack<Id> freeStack;
//	unsigned int count;
//
//	inline Entity *getEntity(Id id){
//		if (entities[id.index].id.version == id.version){
//			return &entities[id.index];
//		} else {
//			return nullptr;
//		}
//	}
//	Id getId(){
//        Id id;
//        if (freeStack.size() == 0){
//            count++;
//            assert (count < ENTITY_COUNT);
//            id.index = count;
//        } else {
//		    id = freeStack.top();
//            id.version += 1;
//		    freeStack.pop();
//        }
//            
//        return id;
//	}
//	void removeId(Id id){
//		if (entities[id.index].id.version == id.version){
//			entities[id.index].id.version += 1;
//			entities[count + 1].id.version += 1;
//
//			// TODO: Better
//			std::swap(entities[id.index].asset, entities[count+1].asset);
//			std::swap(entities[id.index].transform, entities[count+1].transform);
//		}
//	}
//} typedef EntitySlotMap;

#endif
