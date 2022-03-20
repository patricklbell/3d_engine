#ifndef UTILITIES_HPP
#define UTILITIES_HPP
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <GLFW/glfw3.h>
#include <vector>
#ifdef _WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <ShellScalingApi.h>
#endif

// Include Bullet
#include <btBulletDynamicsCommon.h>

const glm::mat4 IDENTITY = glm::mat4();
void mat4_to_float_array(glm::mat4 i_mat, float o_array[16]);
void float_array_to_mat4(glm::mat4& o_mat, float i_array[16]);
void screen_pos_to_world_ray(
    int mouseX, int mouseY,             // Mouse position, in pixels, from bottom-left corner of the window
    int screenWidth, int screenHeight,  // Window size, in pixels
    glm::mat4 ViewMatrix,               // Camera position and orientation
    glm::mat4 ProjectionMatrix,         // Camera parameters (ratio, field of view, near and far planes)
    glm::vec3& out_origin,              // Ouput : Origin of the ray. /!\ Starts at the near plane, so if you want the ray to start at the camera's position instead, ignore this.
    glm::vec3& out_direction            // Ouput : Direction, in world space, of the ray that goes "through" the mouse.
);

//struct Id {
//    unsigned int index;
//    unsigned int version = 0;
//} typedef Id;

struct Entity {
    ModelAsset* asset = nullptr;
    glm::mat4 transform = glm::mat4();
    btRigidBody* rigidbody = nullptr;
    unsigned int id;
} typedef Entity;

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
