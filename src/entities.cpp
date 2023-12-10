#include <vector>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtx/common.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

#include <utilities/math.hpp>

#include "assets.hpp"
#include "entities.hpp"
#include "physics.hpp"
#include "game_behaviour.hpp"

// Used for messages, doesn't change editor state
#include "editor.hpp"

// Updates bones matrices of animesh in accordance with events, if event2 is 
// provided then a linear blending from 1 -> 2 is applied in accordance with bias,
// this function assumes that all parameters are valid and complete
void tickAnimatedMesh(AnimatedMesh* animesh, std::array<glm::mat4, MAX_BONES>& bone_matrices, AnimatedMeshEntity::AnimationEvent* event1, AnimatedMeshEntity::AnimationEvent* event2 = nullptr, float bias = 0.0f, bool blend_transform_forward = true) {
    bool blend = event2 != nullptr;

    for (int i = 0; i < animesh->bone_node_list.size(); ++i) {
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

                //assert(keyframe.id == node.id); // @debug
                node_transform = tickBonesKeyframe(keyframe, event1->current_time * event1->animation->ticks_per_second, event1->loop);
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
                auto lu = event2->animation->bone_id_keyframe_index_map.find(node.id);
                if (lu == event2->animation->bone_id_keyframe_index_map.end()) {
                    std::cerr << "tickAnimatedMesh blended node's bone_index (" << node.id << ") was not mapped to a keyframe, skipping update\n"; // @debug
                }
                else {
                    auto& keyframe = event2->animation->bone_keyframes[lu->second];
                    assert(keyframe.id == node.id); // @debug

                    node_transform_blended = tickBonesKeyframe(keyframe, event2->current_time * event2->animation->ticks_per_second, event2->loop);
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
                if (blend_transform_forward) {
                    if (event1->transform_entity)
                        parent_transform_blended *= event1->delta_transform;
                }
                else {
                    if (event2->transform_entity)
                        parent_transform_blended *= event2->delta_transform;
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
    // Apply any transform that is required
    if (apply_animation_transform) {
        rotation *= animation_delta_rotation;
        position += rotation * animation_delta_position;
        apply_animation_transform = false;

        if (Editor::debug_animations)
            pushInfoMessage("Applied previous animation's transform");
    }


    bool is_default = false, is_first = false;
    AnimatedMeshEntity::AnimationEvent* event = nullptr;
    if (animation_events.size() > 0 && playing_first) {
        event = &animation_events[0];
        is_first = true;
    }
    else if (animation_events.size() > 1) {
        event = &animation_events[1];
    }
    else {
        event = &default_event;
        is_default = true;
    }

    if (event->animation == nullptr) {
        return false;
    }

    bool blend = animation_events.size() > 0 && event->blend;

    // 
    // Animation blending
    //
    float bias = -1.0f; // How much to apply blend animation
    AnimatedMeshEntity::AnimationEvent* blend_event = nullptr;
    bool blend_prev = false;
    if (blend) {
        // @todo multiple blending, might not be necessary
        float progress = event->current_time / event->duration;
        if (progress < event->blend_prev_end && event->blend_prev_end > 0.0f) {
            blend_prev = true;

            if (is_first) {
                blend_event = &default_event;
            }
            else {
                blend_event = &animation_events[0];
            }

            bias = linearstep(event->blend_prev_end, event->blend_prev_start, progress);
            // Kinda hacky way to ensure smooth blending with non blending animations @fix
            if (blend_event->blend)
                bias *= event->blend_prev_amount;
        }
        else if (progress > event->blend_next_start && event->blend_next_start < 1.0f) {
            if (!playing_first && animation_events.size() > 2) {
                blend_event = &animation_events[2];
            }
            else if (playing_first && animation_events.size() > 1) {
                blend_event = &animation_events[1];
            }
            else {
                blend_event = &default_event;
            }

            bias = linearstep(event->blend_next_start, 1.0f, progress);
            // Kinda hacky way to ensure smooth blending with non blending animations @fix
            if (blend_event->blend)
                bias *= event->blend_next_amount;
        }
        else {
            blend = false;
        }
    }
    if (blend_event == nullptr || blend_event->animation == nullptr || bias <= 0.0f) {
        blend = false;
    }

    // Actually tick animation matrices following correct blending
    if (blend) {
        if (blend_prev) {
            // If the transform of the previous event has been applied to entity
            // we need to invert it for blending to reproduce its previous state
            if (blend_event->transform_entity) {
                blend_event->delta_transform = glm::mat4_cast(glm::inverse(blend_event->delta_rotation)) * glm::translate(glm::mat4(1.0), -blend_event->delta_position * glm::inverse(scale));
                blend_event->transform_inverted = true;
            }

            tickAnimatedMesh(animesh, final_bone_matrices, event, blend_event, bias, false);
            blend_state = BlendState::PREVIOUS;
        }
        else {
            if (event->transform_entity) {
                event->delta_transform = glm::mat4_cast(event->delta_rotation) * glm::translate(glm::mat4(1.0), event->delta_position * glm::inverse(scale));
            }

            tickAnimatedMesh(animesh, final_bone_matrices, event, blend_event, bias);
            blend_state = BlendState::NEXT;
        }
    }
    else {
        tickAnimatedMesh(animesh, final_bone_matrices, event);
        blend_state = BlendState::NONE;
    }

    // 
    // Update event times and state
    //
    bool erase_first = false;
    if (blend) {
        if (blend_event->playing)
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
    if (event != nullptr) {
        if (event->playing)
            event->current_time += dt * event->time_scale * time_scale_mult;

        if (event->current_time >= event->duration) {
            if (event->loop) {
                event->current_time = glm::fmod(event->current_time, event->duration);
            }
            else {
                event->current_time = event->duration;
                event->playing = false;

                // Apply transform when next animation starts
                if (event->transform_entity) {
                    apply_animation_transform = true;
                    animation_delta_rotation = event->delta_rotation;
                    animation_delta_position = event->delta_position;

                    if (Editor::debug_animations)
                        pushInfoMessage("Buffering animation " + event->animation->name + "'s transform");
                }

                if (!is_default) {
                    if (!playing_first) {
                        erase_first = true;
                    }
                    playing_first = false;
                }

                // Account for blending with next in blend prev end
                if (blend_event != nullptr) {
                    blend_event->blend_prev_start = blend_event->current_time / blend_event->duration;
                    blend_event->blend_prev_end += blend_event->blend_prev_start;
                }
            }
        }
    }

    // @debug
    if (Editor::debug_animations) {
        auto tmpstr = blend_prev ? "previous animation " : "next animation ";
        if (event->animation != nullptr)
            std::cout << "Animation " << event->animation->name << " (" << event->current_time / event->duration << ") ";
        if (blend && blend_event->animation != nullptr)
            std::cout << " blending with " << tmpstr << blend_event->animation->name << " (" << blend_event->current_time / blend_event->duration << ") " << " with bias " << bias;
        std::cout << "\n";
    }

    // @note IMPORTANT pointers are invalidated after this
    if (erase_first) {
        animation_events.erase(animation_events.begin());
    }

    return event->playing;
}

void AnimatedMeshEntity::init() {
    tick(0.0f);
}

AnimatedMeshEntity::AnimationEvent* AnimatedMeshEntity::play(const std::string& name, float start_time, bool fallback, bool immediate, bool playing) {
    if (animesh == nullptr) {
        std::cerr << "Failed to play animation " << name << " because no animation asset was loaded\n"; // @debug
        return nullptr;
    }

    auto lu = animesh->name_animation_map.find(name);
    if (lu == animesh->name_animation_map.end()) {
        std::cerr << "Failed to play animation " << name << " because it wasn't loaded\n"; // @debug
        return nullptr;
    }

    if (immediate) {
        playing_first = false;

        if (animation_events.size() > 0)
            animation_events.resize(1);
    }

    AnimatedMeshEntity::AnimationEvent* event;
    if (fallback) {
        event = &default_event;
    }
    else {
        // If we are playing the fallback but there is a previous animation buffered, clear it
        if (!playing_first && animation_events.size() == 1) {
            animation_events.clear();
        }

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
    if (actions.size() > MAX_ACTION_BUFFER)
        return false;

    static const glm::quat rotate_left(glm::vec3(0.0, PI / 2.0, 0.0));

    auto& a = actions.emplace_back();
    a.type = PlayerActionType::TURN_LEFT;
    a.duration = 0.5;
    a.time = 0.0;

    a.delta_position = glm::vec3(0.0);
    a.delta_rotation = rotate_left;

    if (Editor::debug_animations)
        pushInfoMessage("Added TURN_LEFT action");
    return true;
}

bool PlayerEntity::turn_right() {
    if (actions.size() > MAX_ACTION_BUFFER)
        return false;

    static const glm::quat rotate_right(glm::vec3(0.0, -PI / 2.0, 0.0));

    auto& a = actions.emplace_back();
    a.type = PlayerActionType::TURN_RIGHT;
    a.duration = 0.5;
    a.time = 0.0;

    a.delta_position = glm::vec3(0.0);
    a.delta_rotation = rotate_right;

    if (Editor::debug_animations)
        pushInfoMessage("Added TURN_RIGHT action");
    return true;
}

bool PlayerEntity::step_forward() {
    if (actions.size() > MAX_ACTION_BUFFER)
        return false;

    auto& a = actions.emplace_back();
    a.type = PlayerActionType::STEP_FORWARD;
    a.duration = 0.6;
    a.time = 0.0;

    a.delta_position = glm::vec3(0.0, 0.0, 1.0);
    a.delta_rotation = glm::quat();

    if (Editor::debug_animations)
        pushInfoMessage("Added STEP_FORWARD action");
    return true;
}


void tickEntities(EntityManager& entities, float dt, bool is_playing) {
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        auto e = entities.entities[i];
        if (e == nullptr) continue;

        if (entityInherits(e->type, EntityType::MESH_ENTITY)) {
            auto me = reinterpret_cast<MeshEntity*>(e);
            
            if (is_playing && !me->body_id.IsInvalid()) {
                auto& body_interface = physics::system->GetBodyInterfaceNoLock();
                auto pos = body_interface.GetCenterOfMassPosition(me->body_id);
                auto rot = body_interface.GetRotation(me->body_id).GetXYZW();

                me->position = std::move(glm::vec3(pos.mValue[0], pos.mValue[1], pos.mValue[2]));
                me->rotation = std::move(glm::quat(rot.mValue[0], rot.mValue[1], rot.mValue[2], rot.mValue[3]));
            }
        }
        if (entityInherits(e->type, EntityType::ANIMATED_MESH_ENTITY)) {
            auto ae = reinterpret_cast<AnimatedMeshEntity*>(e);

            if (is_playing || ae->draw_animated) {
                ae->tick(dt);
            }
        }
    }

    if (is_playing) {
        updateCameraMove(dt);

        if (entities.player != NULLID) {
            auto player = (PlayerEntity*)entities.getEntity(entities.player);
            if(player != nullptr) 
                updatePlayerEntity(entities, dt, *player);
        }
    }
}
