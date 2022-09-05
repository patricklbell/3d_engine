#ifndef EDITOR_H
#define EDITOR_H

#include <string>
#include <iostream>
#include <stack>
#include <map>

#include <glm/glm.hpp>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imfilebrowser.hpp"
#include "graphics.hpp"
#include "utilities.hpp"
#include "entities.hpp"

enum class TransformType : uint64_t {
    NONE = 0,
    POS = 1,
    ROT = 2,
    SCL = 4,
    POS_ROT = 3,
    POS_SCL = 5,
    ROT_SCL = 6,
    ALL = 7
};

void initEditorGui(AssetManager &asset_manager);

void ImTerminal(EntityManager &entity_manager, AssetManager &asset_manager, bool is_active, Camera &level_camera, Camera &editor_camera);

void drawEditorGui(Camera& editor_camera, Camera& level_camera, EntityManager& entity_manager, AssetManager& asset_manager);
void drawGameGui(Camera& editor_camera, Camera& level_camera, EntityManager& entity_manager, AssetManager& asset_manager);

bool editorTranslationGizmo(glm::vec3 &pos, glm::quat &rot, glm::mat3 &scl, Camera &camera, const glm::vec3 &snap, bool do_snap);
bool editorRotationGizmo(glm::vec3 &pos, glm::quat &rot, glm::mat3 &scl, const Camera &camera, float rot_snap, bool do_snap);
bool editorScalingGizmo(glm::vec3 &pos, glm::quat &rot, glm::mat3 &scl, Camera &camera, const glm::vec3 &snap, bool do_snap);
TransformType editTransform(Camera &camera, glm::vec3 &pos, glm::quat &rot, glm::mat3 &scl, glm::vec3 &snap, TransformType type);
void drawWaterDebug(WaterEntity* w, const Camera &camera, bool flash);
void drawMeshCube(const glm::vec3 &pos, const glm::quat &rot, const glm::mat3x3 &scl, const Camera &camera);
void drawFrustrum(Camera& drawn_camera, const Camera& camera);
void drawMeshWireframe(const Mesh &mesh, const glm::vec3 &pos, const glm::quat &rot, const glm::mat3x3 &scl, const Camera &camera, bool flash);
void drawEditor3DRing(const glm::vec3 &position, const glm::vec3 &direction, const Camera &camera, const glm::vec4 &color, const glm::vec3 &scale, bool shaded=true);
void drawEditor3DArrow(const glm::vec3 &position, const glm::vec3 &direction, const Camera &camera, const glm::vec4 &color, const glm::vec3 &scale, bool shaded=true, bool block=false);
void drawColliders(const EntityManager& entity_manager, const Camera& camera);

struct ReferenceSelection {
    void addEntity(Entity* e);
    void toggleEntity(const EntityManager& entity_manager, Entity* e);
    void clear();

    std::vector<Id> ids;
    EntityType type = ENTITY;
    glm::vec3 avg_position = glm::vec3(0.0);
    int avg_position_count = 0;
};
struct CopySelection {
    // @note owns memory
    std::vector<Entity*> entities;
    
    void free_clear();
    ~CopySelection();
};

void referenceToCopySelection(EntityManager& entity_manager, const ReferenceSelection& ref, CopySelection& cpy);
void createCopySelectionEntities(EntityManager& entity_manager, AssetManager& asset_manager, CopySelection& cpy, ReferenceSelection& ref);

enum class GizmoMode {
    TRANSLATE = 0,
    ROTATE,
    SCALE,
    NONE,
};

enum class EditorMode {
    ENTITY = 0,
    COLLIDERS,
    NUM,
};

namespace editor {
    extern EditorMode editor_mode;

    extern GizmoMode gizmo_mode;
    extern glm::vec3 translation_snap;

    extern ImGui::FileBrowser im_file_dialog;
    extern std::string im_file_dialog_type;

    extern bool do_terminal;
    extern bool draw_debug_wireframe;
    extern bool draw_colliders;
    extern bool transform_active;
    extern bool use_level_camera, draw_level_camera;

    extern ReferenceSelection selection;
    extern CopySelection copy_selection;
}


#endif
