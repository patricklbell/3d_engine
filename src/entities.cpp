#include <vector>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtx/common.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "entities.hpp"
#include "assets.hpp"
#include "utilities.hpp"

EntityManager level_entity_manager;
EntityManager game_entity_manager;

// Updates bones matrices of animesh in accordance with events, if event2 is 
// provided then a linear blending from 1 -> 2 is applied in accordance, this
// function assumes that all parameters are valid and complete
void tickAnimatedMesh(AnimatedMesh* animesh, std::array<glm::mat4, MAX_BONES> &bone_matrices, AnimatedMeshEntity::AnimationEvent* event1, AnimatedMeshEntity::AnimationEvent* event2=nullptr, float bias=0.0f, bool apply_blend_transform = true) {
    bool blend = event2 != nullptr;

    for(int i = 0; i < animesh->bone_node_list.size(); ++i) {
        auto& node = animesh->bone_node_list[i];
        auto& aninode = animesh->bone_node_animation_list[i];

        auto node_transform = node.local_transform; // @speed

        // @debug, should be covered by lookup
        if (node.id != (uint64_t)-1) {
            auto lu = event1->animation->bone_id_keyframe_index_map.find(node.id);
            if (lu == event1->animation->bone_id_keyframe_index_map.end()) {
                std::cerr << "tickAnimatedMesh node's bone_index (" << node.id << ") was not mapped to a keyframe, skipping update\n"; // @debug
            }
            else {
                auto& keyframe = event1->animation->bone_keyframes[lu->second];

                assert(keyframe.id == node.id); // @debug
                tickBonesKeyframe(keyframe, event1->current_time * event1->animation->ticks_per_second, event1->loop);
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
        if (blend) {
            auto node_transform_blended = node.local_transform;

            if (node.id != (uint64_t)-1) {
                auto lu2 = event2->animation->bone_id_keyframe_index_map.find(node.id);
                if (lu2 == event2->animation->bone_id_keyframe_index_map.end()) {
                    std::cerr << "tickAnimatedMesh blended node's bone_index (" << node.id << ") was not mapped to a keyframe, skipping update\n"; // @debug
                } else {
                    auto& keyframe2 = event2->animation->bone_keyframes[lu2->second];
                    assert(keyframe2.id == node.id); // @debug

                    tickBonesKeyframe(keyframe2, event2->current_time * event1->animation->ticks_per_second, event2->loop);
                    node_transform_blended = keyframe2.local_transformation;
                }
            }

            glm::mat4 parent_transform_blended;
            if (node.parent_index != -1) {
                parent_transform_blended = animesh->bone_node_animation_list[node.parent_index].global_transform_blended;
            }
            else {
                parent_transform_blended = animesh->global_transform;

                // If there is a transform that will be applied when the animation completes then we
                // want to apply this to the animation we are blending with
                if (apply_blend_transform && event1->transform_animation) {
                    parent_transform_blended *= event1->delta_transform;
                }
            }

            aninode.global_transform_blended = parent_transform_blended * node_transform_blended;
        }

        if (node.id != (uint64_t)-1) {
            // @debug
            if (node.id < 0 || node.id >= MAX_BONES || node.id >= animesh->bone_offsets.size()) {
                std::cerr << "tickAnimatedMesh node's id (" << node.id << ") was not set or invalid, skipping update\n";
            }
            else {
                if (blend) {
                    bone_matrices[node.id] = lerpMatrix(aninode.global_transform, aninode.global_transform_blended, bias) * animesh->bone_offsets[node.id];
                }
                else {
                    bone_matrices[node.id] = aninode.global_transform * animesh->bone_offsets[node.id];
                }
            }
        }
    }
}

// Returns true if animation is playing
bool AnimatedMeshEntity::tick(float dt) {
    bool is_default = false;
    AnimatedMeshEntity::AnimationEvent* event = nullptr;
    if (animation_events.size() > 0 && playing_first) {
        event = &animation_events[0];
    } else if (animation_events.size() > 1) {
        event = &animation_events[1];
    } else {
        event = &default_event;
        is_default = true;
    }

    bool blend = animation_events.size() > 0 && event->blend;
    if (animation_events.size() == 1 && default_event.animation == nullptr)
        blend = false;

    // 
    // Animation blending
    //
    float bias = -1.0f; // How much to apply blend animation
    AnimatedMeshEntity::AnimationEvent* blend_event = nullptr;
    bool blend_prev = false;
    if (blend) {
        // @todo multiple blending, might not be necessary
        float progress = event->current_time / event->duration;
        if (progress < event->blend_prev_end) {
            if (playing_first) {
                blend_event = &default_event;
            } else {
                blend_event = &animation_events[0];
            }

            bias = linearstep(0.0f, event->blend_prev_end, progress);
            // Kinda hacky way to fully blend with default state @fix
            if (blend_event != &default_event)
                bias = linearstep(event->blend_prev_amount, 1.0, bias);

            blend_prev = true;
        } else if (progress > event->blend_next_start) {
            if (!playing_first && animation_events.size() > 2) {
                blend_event = &animation_events[2];
            } else if (playing_first && animation_events.size() > 1) {
                blend_event = &animation_events[1];
            } else {
                blend_event = &default_event;
            }

            bias = linearstep(event->blend_next_start, 1.0f, progress);
            // Kinda hacky way to fully blend with default state @fix
            if (blend_event != &default_event)
                bias *= event->blend_next_amount;

        } else {
            blend = false;
        }
    }

    // @debug
    auto tmpstr = blend_prev ? "previous animation " : "next animation ";
    if (event->animation != nullptr)
        std::cout << "Animation " << event->animation->name << " (" << event->current_time / event->duration << ") ";
    if (blend && blend_event->animation != nullptr)
        std::cout << " blending with " << tmpstr << blend_event->animation->name << " (" << blend_event->current_time / blend_event->duration << ") " << " with bias " << bias;
    std::cout << "\n";

    // Actually tick animation matrices following correct blending
    if (bias <= 0.0f) {
        if(event->animation != nullptr)
            tickAnimatedMesh(animesh, final_bone_matrices, event);
    } else if (bias >= 1.0f) {
        if (blend_event->animation != nullptr)
            tickAnimatedMesh(animesh, final_bone_matrices, blend_event);
    } else if (blend && blend_event->animation != nullptr && event->animation != nullptr) {
        if (blend_prev) {
            // @fix If the transform of the previous event has been applied to entity
            // we need to invert it for blending to reproduce its previous state
            // This is only needed if the previous animation is reset
            /*if (blend_event->transform_entity && !blend_event->transform_inverted) {
                blend_event->delta_transform = glm::inverse(blend_event->delta_transform);
                blend_event->transform_animation = true;
            }*/

            tickAnimatedMesh(animesh, final_bone_matrices, event, blend_event, 1.0 - bias, false);
        } else {
            tickAnimatedMesh(animesh, final_bone_matrices, event, blend_event, bias);
        }
    }

    // 
    // Update events times and state
    //
    if (blend && bias > 0.0f && blend_event->animation != nullptr && blend_event->playing) {
        blend_event->current_time += dt * blend_event->time_scale * time_scale_mult;

        if (blend_event->current_time >= blend_event->duration) {
            if (blend_event->loop) {
                blend_event->current_time = glm::fmod(blend_event->current_time, blend_event->duration);
            }
            else {
                blend_event->current_time = blend_event->duration;
                blend_event->playing = false;
            }
        }
    }
    if (event != nullptr && event->animation != nullptr && event->playing) {
        event->current_time += dt * event->time_scale * time_scale_mult;

        if (event->current_time >= event->duration) {
            if (event->loop) {
                event->current_time = glm::fmod(event->current_time, event->duration);
            }
            else {
                event->current_time = event->duration;
                event->playing = false;

                if (!is_default) {
                    if (!playing_first) {
                        animation_events.erase(animation_events.begin());
                    }
                    playing_first = false;
                }

                // Clear the state of the next event since blending to it has changed it
                //if (blend_event != nullptr) {
                //    blend_event->current_time = blend_event->start_time;
                //    // @note this could be better
                //    if(blend_event->current_time > blend_event->start_time)
                //        blend_event->playing = true;
                //}

                // Apply transform when animation completes
                if (event->transform_entity) {
                    rotation *= event->delta_rotation;
                    position += rotation * event->delta_position;
                }
            }
        }
    }

    return event->playing;
}

void AnimatedMeshEntity::init() {
    tick(0.0f);
}

AnimatedMeshEntity::AnimationEvent *AnimatedMeshEntity::play(const std::string& name, float start_time, bool fallback, bool immediate, bool playing) {
    auto lu = animesh->name_animation_map.find(name);
    if (lu == animesh->name_animation_map.end()) {
        std::cerr << "Failed to play animation " << name << " because it wasn't loaded\n"; // @debug
        return nullptr;
    }
    
    if (immediate) {
        if (fallback)
            playing_first = false;

        if(animation_events.size() > 0)
            animation_events.resize(1);
    }

    // If we are playing the fallback but there is a previous animation buffered, clear it
    if (!playing_first && animation_events.size() == 1) {
        animation_events.clear();
    }

    AnimatedMeshEntity::AnimationEvent* event;
    if (fallback) {
        event = &default_event;
    } else {
        if (animation_events.size() == 0) {
            playing_first = true;
        }
        event = &animation_events.emplace_back();
    }

    event->animation = &lu->second;
    event->duration = lu->second.duration / (float)event->animation->ticks_per_second;
    event->playing = playing;
    event->start_time = start_time;
    event->current_time = start_time;

    return event;
}

bool PlayerEntity::turn_left() {
    if (actions.size() > MAX_ACTION_BUFFER ||
        (actions.size() > 0 && actions[actions.size()-1].type == PlayerActionType::TURN_LEFT))
        return false;

    static const glm::quat rotate_left(glm::vec3(0.0, PI / 2.0, 0.0));

    auto& a = actions.emplace_back();
    a.type = PlayerActionType::TURN_LEFT;
    a.duration = 0.4;
    a.time = 0.0;

    a.delta_position = glm::vec3(0.0);
    a.delta_rotation = rotate_left;

    return true;
}

bool PlayerEntity::turn_right() {
    if (actions.size() > MAX_ACTION_BUFFER ||
        (actions.size() > 0 && actions[actions.size()-1].type == PlayerActionType::TURN_RIGHT))
        return false;

    static const glm::quat rotate_right(glm::vec3(0.0, -PI / 2.0, 0.0));

    auto& a = actions.emplace_back();
    a.type = PlayerActionType::TURN_RIGHT;
    a.duration = 0.4;
    a.time = 0.0;

    a.delta_position = glm::vec3(0.0);
    a.delta_rotation = rotate_right;

    return true;
}

bool PlayerEntity::step_forward() {
    if (actions.size() > MAX_ACTION_BUFFER ||
        (actions.size() > 0 && actions[actions.size()-1].type == PlayerActionType::STEP_FORWARD))
        return false;

    auto &a = actions.emplace_back();
    a.type = PlayerActionType::STEP_FORWARD;
    a.duration = 1.0;
    a.time = 0.0;

    a.delta_position = glm::vec3(0.0, 0.0, 1.0);
    a.delta_rotation = glm::quat();

    return true;
}