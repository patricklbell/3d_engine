#ifndef UTILITIES_HPP
#define UTILITIES_HPP

#include <string>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include <btBulletDynamicsCommon.h>
#include "assets.hpp"

void float_array_to_mat4(glm::mat4& o_mat, float i_array[16]);
void screenPosToWorldRay(glm::ivec2 mouse_position, glm::mat4 view, glm::mat4 projection, glm::vec3 &out_origin, glm::vec3 &out_direction);
struct Entity {
    ModelAsset* asset = nullptr;
    glm::mat4 transform = glm::mat4();
    btRigidBody* rigidbody = nullptr;
    unsigned int id;
} typedef Entity;


//
//  Potentially more advanced implementation of entity storage
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

bool load_mtl(Material * mat, const std::string &path);
void load_asset(ModelAsset * asset, const std::string &objpath, const std::string &mtlpath);

#endif /* ifndef UTILITIES_HPP */
