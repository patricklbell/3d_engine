#include <vector>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtx/common.hpp>

#include "entities.hpp"
#include "assets.hpp"

void tickAnimatedMesh(AnimatedMeshEntity& entity, float time, bool looping) {
    auto& animesh = entity.animesh;
    auto& animation = entity.animation;
    for (auto& node : animesh->bone_node_list) {
        auto node_transform = node.local_transform; // @speed

        // @debug, should be covered by lookup
        if (node.id != (uint64_t)-1) {
            auto lu = animation->bone_id_keyframe_index_map.find(node.id);
            if (lu == animation->bone_id_keyframe_index_map.end()) {
                std::cerr << "tickAnimatedMesh node's bone_index (" << node.id << ") was not mapped to a keyframe, skipping update\n";
            }
            else {
                auto keyframe_index = lu->second;

                assert(animation->bone_keyframes[keyframe_index].id == node.id); // @debug
                tickBonesKeyframe(animation->bone_keyframes[keyframe_index], time, looping);
                node_transform = animation->bone_keyframes[keyframe_index].local_transformation;
            }
        }

        glm::mat4 parent_transform;
        if (node.parent_index != -1) {
            parent_transform = animesh->bone_node_list[node.parent_index].global_transform;
        }
        else {
            parent_transform = animesh->global_transform;
        }

        node.global_transform = parent_transform * node_transform;

        if (node.id != (uint64_t)-1) {
            // @debug
            if (node.id < 0 || node.id >= MAX_BONES || node.id >= animesh->bone_offsets.size()) {
                std::cerr << "tickAnimatedMesh node's id (" << node.id << ") was not set or invalid, skipping update\n";
            }
            else {
                entity.final_bone_matrices[node.id] = node.global_transform * animesh->bone_offsets[node.id];
            }
        }
    }
}

// Returns true if animation is playing
bool AnimatedMeshEntity::tick(float dt) {
    if (animation != nullptr && playing) {
        current_time += dt * time_scale;
        if (current_time >= animation->duration) {
            if (loop) {
                //std::cout << "Looping animation\n";
                current_time = glm::fmod(current_time, animation->duration);
            }
            else {
                current_time = 0.0f;
                playing = false;
            }
        }
        //std::cout << "Ticking animation " << animation->name << ", current time is " << current_time << "\n";
        tickAnimatedMesh(*this, current_time, loop);

        return current_time < animation->duration;
    }
    return false;
}

// Returns true if successfully found animation
bool AnimatedMeshEntity::play(const std::string& name, float start_time = 0.0f, float _time_scale = 1.0f, bool _loop = false) {
    auto lu = animesh->name_animation_map.find(name);
    if (lu == animesh->name_animation_map.end()) {
        std::cerr << "Failed to play animation " << name << " because it wasn't loaded\n"; // @debug
        return false;
    }
    else {
        loop = _loop;
        time_scale = _time_scale;
        current_time = start_time;
        animation = &lu->second;
        playing = true;
    }
    return true;
}