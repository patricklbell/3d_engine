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

enum class TransformType : unsigned int{
    POS = 1,
    ROT = 2,
    SCL = 4,
    POS_ROT = 3,
    POS_SCL = 5,
    ROT_SCL = 6,
    ALL = 7
};

void initEditorGui(AssetManager &asset_manager);

void ImTerminal(EntityManager &entity_manager, AssetManager &asset_manager, bool is_active);

void drawEditorGui(Camera &camera, EntityManager &entity_manager, AssetManager &asset_manager);

bool editorTranslationGizmo(glm::vec3 &pos, glm::quat &rot, glm::mat3 &scl, Camera &camera, const glm::vec3 &snap, bool do_snap);
bool editorRotationGizmo(glm::vec3 &pos, glm::quat &rot, glm::mat3 &scl, const Camera &camera, float rot_snap, bool do_snap);
bool editorScalingGizmo(glm::vec3 &pos, glm::quat &rot, glm::mat3 &scl, Camera &camera, const glm::vec3 &snap, bool do_snap);
bool editTransform(Camera &camera, glm::vec3 &pos, glm::quat &rot, glm::mat3 &scl, glm::vec3 &snap, TransformType type);
void drawEditor3DArrow(const glm::vec3 &position, const glm::vec3 &direction, const Camera &camera, const glm::vec4 &color, const glm::vec3 &scale, bool shaded=true, bool block=false);
void drawEditor3DRing(const glm::vec3 &position, const glm::vec3 &direction, const Camera &camera, const glm::vec4 &color, const glm::vec3 &scale, bool shaded=true);
void drawMeshCube(const glm::vec3 &pos, const glm::quat &rot, const glm::mat3x3 &scl, const Camera &camera);
void drawMeshWireframe(const Mesh &mesh, const glm::vec3 &pos, const glm::quat &rot, const glm::mat3x3 &scl, const Camera &camera, bool flash);
void drawWaterDebug(WaterEntity* w_e, const Camera &camera, bool flash);

namespace editor {
    extern enum GizmoMode {
        GIZMO_MODE_TRANSLATE = 0,
        GIZMO_MODE_ROTATE,
        GIZMO_MODE_SCALE,
        GIZMO_MODE_NONE,
    } gizmo_mode;
    extern std::string im_file_dialog_type;
    extern bool do_terminal;
    //extern GLDebugDrawer bt_debug_drawer;
    //extern bool draw_bt_debug;
    extern ImGui::FileBrowser im_file_dialog;
    extern bool draw_debug_wireframe;
    extern bool transform_active;
    extern Id sel_e;
    extern AssetManager editor_assets;
    extern glm::vec3 translation_snap;
}

#endif
