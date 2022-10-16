#include <vector>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtx/common.hpp>

#include "entities.hpp"
#include "assets.hpp"
#include "utilities.hpp"

EntityManager level_entity_manager;
EntityManager game_entity_manager;

void tickAnimatedMesh(AnimatedMeshEntity& entity, float time, bool looping, const bool blended=false, float time2=0.0f, float bias=0.0f) {
    auto& animesh = entity.animesh;
    auto& animation = entity.animation;
    auto& animation2 = entity.animation_blend_to;

    for(int i = 0; i < animesh->bone_node_list.size(); ++i) {
        auto& node = animesh->bone_node_list[i];
        auto& aninode = animesh->bone_node_animation_list[i];

        auto node_transform = node.local_transform; // @speed

        // @debug, should be covered by lookup
        if (node.id != (uint64_t)-1) {
            auto lu = animation->bone_id_keyframe_index_map.find(node.id);
            if (lu == animation->bone_id_keyframe_index_map.end()) {
                std::cerr << "tickAnimatedMesh node's bone_index (" << node.id << ") was not mapped to a keyframe, skipping update\n";
            }
            else {
                auto& keyframe = animation->bone_keyframes[lu->second];

                assert(keyframe.id == node.id); // @debug
                tickBonesKeyframe(keyframe, time, looping);
                node_transform = keyframe.local_transformation;
            }
        }

        glm::mat4 parent_transform;
        if (node.parent_index != -1) {
            parent_transform = animesh->bone_node_animation_list[node.parent_index].global_transform;
        }
        else {
            parent_transform = animesh->global_transform;
        }

        aninode.global_transform = parent_transform * node_transform;

        // Compute blended component, probably a better way to do this
        if (blended && bias != 0.0f) {
            auto node_transform_blended = node.local_transform;

            if (node.id != (uint64_t)-1) {
                auto lu2 = animation2->bone_id_keyframe_index_map.find(node.id);
                if (lu2 == animation2->bone_id_keyframe_index_map.end()) {
                    std::cerr << "tickAnimatedMesh blended node's bone_index (" << node.id << ") was not mapped to a keyframe, skipping update\n";
                }
                else {
                    auto& keyframe2 = animation2->bone_keyframes[lu2->second];
                    assert(keyframe2.id == node.id); // @debug

                    tickBonesKeyframe(keyframe2, time2, looping);
                    node_transform_blended = keyframe2.local_transformation;
                }
            }

            glm::mat4 parent_transform_blended;
            if (node.parent_index != -1) {
                parent_transform_blended = animesh->bone_node_animation_list[node.parent_index].global_transform_blended;
            }
            else {
                parent_transform_blended = animesh->global_transform*entity.transform_blend_to;
            }

            aninode.global_transform_blended = parent_transform_blended * node_transform_blended;
        }

        if (node.id != (uint64_t)-1) {
            // @debug
            if (node.id < 0 || node.id >= MAX_BONES || node.id >= animesh->bone_offsets.size()) {
                std::cerr << "tickAnimatedMesh node's id (" << node.id << ") was not set or invalid, skipping update\n";
            }
            else {
                if (blended && bias != 0.0f) {
                    entity.final_bone_matrices[node.id] = lerpMatrix(aninode.global_transform, aninode.global_transform_blended, bias) * animesh->bone_offsets[node.id];
                }
                else {
                    entity.final_bone_matrices[node.id] = aninode.global_transform * animesh->bone_offsets[node.id];
                }
            }
        }
    }
}

// Returns true if animation is playing
bool AnimatedMeshEntity::tick(float dt) {
    if (animation != nullptr && playing) {
        current_time += dt * time_scale * animation->ticks_per_second;
        if (current_time >= animation->duration) {
            if (loop) {
                //std::cout << "Looping animation\n";
                current_time = glm::fmod(current_time, animation->duration);
            }
            else {
                current_time = animation->duration;
                playing = false;
            }
        }

        if (blending) {
            current_bias = glm::smoothstep(bias, 1.0f, current_time / animation->duration);
            std::cout << "Using blended tick, ratio: " << current_time / animation->duration << " bias: " << current_bias << "\n";

            if (current_bias >= 1.0f) {
                animation = animation_blend_to;
                current_time = current_time_blend_to;
                time_scale = time_scale_blend_to;
                blending = false;
            } else {
                current_time_blend_to += dt * time_scale_blend_to * animation_blend_to->ticks_per_second;
                if (current_time_blend_to >= animation_blend_to->duration) {
                    if (loop) {
                        current_time_blend_to = glm::fmod(current_time_blend_to, animation_blend_to->duration);
                    }
                    else {
                        current_time_blend_to = animation_blend_to->duration;
                    }
                }
                tickAnimatedMesh(*this, current_time, loop, true, current_time_blend_to, current_bias);
            }
        } else {
            tickAnimatedMesh(*this, current_time, loop);
        }

        //std::cout << "Ticking animation " << animation->name << ", current time is " << current_time << "\n";

        return current_time < animation->duration;
    }
    return false;
}

void AnimatedMeshEntity::init() {
    if(blending)
        tickAnimatedMesh(*this, current_time, loop, true, current_time_blend_to, current_bias);
    else
        tickAnimatedMesh(*this, current_time, loop);
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

bool AnimatedMeshEntity::playBlended(const std::string& name1, float start_time1 = 0.0f, float _time_scale1 = 1.0f,
                                     const std::string& name2="", float start_time2 = 0.0f, float _time_scale2 = 1.0f,
                                     glm::mat4 delta_transform=glm::mat4(1.0),
                                     float bias = 0.5f, bool _loop = false) {
    auto lu1 = animesh->name_animation_map.find(name1);
    auto lu2 = animesh->name_animation_map.find(name2);
    if (lu1 == animesh->name_animation_map.end()) {
        std::cerr << "Failed to play animation " << name1 << " because it wasn't loaded\n"; // @debug
        return false;
    }
    else if (lu2 == animesh->name_animation_map.end()) {
        std::cerr << "Failed to play animation " << name2 << " because it wasn't loaded\n"; // @debug
        return false;
    }
    else {
        loop = _loop;

        time_scale = _time_scale1;
        time_scale_blend_to = _time_scale2;

        current_time = start_time1;
        current_time_blend_to = start_time2;

        animation = &lu1->second;
        animation_blend_to = &lu2->second;

        current_bias = 0.0f;
        bias = bias;

        transform_blend_to = delta_transform;

        playing = true;
        blending = true;
    }
    return true;
}

float AnimatedMeshEntity::getAnimationDuration(const std::string& name) {
    auto lu = animesh->name_animation_map.find(name);
    if (lu == animesh->name_animation_map.end()) {
        std::cerr << "Failed to get animation duration " << name << " because it wasn't loaded\n"; // @debug
        return 0.0;
    }
    else {
        return lu->second.duration / lu->second.ticks_per_second;
    }
}

bool PlayerEntity::turn_left() {
    if (actions.size() > MAX_ACTION_BUFFER ||
        (actions.size() > 0 && actions[0].type == PlayerActionType::TURN_LEFT))
        return false;

    static const glm::quat rotate_left(glm::vec3(0.0, PI / 2.0, 0.0));

    PlayerAction a;
    a.type = PlayerActionType::TURN_LEFT;
    a.duration = 0.4;
    a.time = 0.0;

    a.delta_position = glm::vec3(0.0);
    a.delta_rotation = rotate_left;
    
    actions.emplace_back(std::move(a));
    return true;
}

bool PlayerEntity::turn_right() {
    if (actions.size() > MAX_ACTION_BUFFER ||
        (actions.size() > 0 && actions[0].type == PlayerActionType::TURN_RIGHT))
        return false;

    static const glm::quat rotate_right(glm::vec3(0.0, -PI / 2.0, 0.0));

    PlayerAction a;
    a.type = PlayerActionType::TURN_RIGHT;
    a.duration = 0.4;
    a.time = 0.0;

    a.delta_position = glm::vec3(0.0);
    a.delta_rotation = rotate_right;

    actions.emplace_back(std::move(a));
    return true;
}

bool PlayerEntity::step_forward() {
    if (actions.size() > MAX_ACTION_BUFFER ||
        (actions.size() > 0 && actions[0].type == PlayerActionType::STEP_FORWARD))
        return false;

    PlayerAction a;
    a.type = PlayerActionType::STEP_FORWARD;
    a.duration = 1.0;
    a.time = 0.0;

    a.delta_position = glm::vec3(0.0, 0.0, 1.0);
    a.delta_rotation = glm::quat();

    actions.emplace_back(std::move(a));
    return true;
}