#include "globals.hpp"

#include <stack>
#include <limits>
#include <algorithm>
#include <filesystem>
#include <functional>

// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <GLFW/glfw3.h>

// Include ImGui
#include "glm/detail/func_geometric.hpp"
#include "glm/detail/type_mat.hpp"
#include "glm/fwd.hpp"
#include "glm/gtc/quaternion.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_stdlib.h"
#include "imfilebrowser.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <string>

#include <controls/globals.hpp>
#include <camera/globals.hpp>
#include <shader/globals.hpp>
#include <utilities/math.hpp>
#include <utilities/strings.hpp>

#include "renderer.hpp"
#include "editor.hpp"
#include "graphics.hpp"
#include "texture.hpp"
#include "assets.hpp"
#include "entities.hpp"
#include "level.hpp"
#include "game_behaviour.hpp"
#include "lightmapper.hpp"

namespace Editor {
    Camera editor_camera;

    EditorMode editor_mode = EditorMode::ENTITY;
    Mesh arrow_mesh;
    Mesh block_arrow_mesh;
    Mesh ring_mesh;

    GizmoMode gizmo_mode = GizmoMode::NONE;
    glm::vec3 translation_snap = glm::vec3(1.0);

    ImGui::FileBrowser im_file_dialog = ImGui::FileBrowser(ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_NoTitleBar);
    std::string im_file_dialog_type;

    bool do_terminal;
    bool draw_debug_wireframe = true;
    bool draw_colliders = false;
    bool transform_active = false;
    bool use_level_camera = false, draw_level_camera = false;
    bool debug_animations = false;

    ReferenceSelection selection;
    CopySelection copy_selection;

    std::unordered_map<uint64_t, std::string> entity_type_to_string;
    std::vector<InfoMessage> info_message_queue;
}
using namespace Editor;

void pushInfoMessage(std::string contents, InfoMessage::Urgency urgency, float duration, std::string id) {
    InfoMessage *m = nullptr;
    if (id != "") {
        for (auto& existing_m : info_message_queue) {
            if (existing_m.id == id) {
                m = &existing_m;
                break;
            }
        }
    }

    if (m == nullptr) {
        m = &info_message_queue.emplace_back();;
    }

    m->contents = contents;
    m->urgency = urgency;
    m->duration = duration;
    m->id = id;
    m->time = glfwGetTime();
}

void ReferenceSelection::addEntity(Entity* e) {
    submesh_i = -1;

    for (const auto &id : ids) {
        if (id == e->id) return;
    }

    ids.push_back(e->id);
    if (type == ENTITY) type = e->type;
    else                type = (EntityType)(type & e->type);

    if (entityInherits(e->type, MESH_ENTITY)) {
        auto pos = ((MeshEntity*)e)->position + ((MeshEntity*)e)->gizmo_position_offset;
        avg_position = (float)avg_position_count * avg_position + pos;
        avg_position_count++;
        avg_position /= (float)avg_position_count;
    }
}

bool ReferenceSelection::setSubmesh(int i) {
    if (ids.size() == 1) {
        submesh_i = i;
        return false;
    }
    return true;
}

// If entity is not already in selection add it, else remove it
void ReferenceSelection::toggleEntity(const EntityManager &entity_manager, Entity* e) {
    submesh_i = -1;

    int id_to_erase = -1;
    for (int i = 0; i < ids.size(); ++i) {
        const auto& id = ids[i];
        if (id == e->id) {
            id_to_erase = i;
        }
        else {
            auto e = entity_manager.getEntity(id);
            if (e == nullptr) continue;

            if (i == 0) type = e->type;
            else        type = (EntityType)(type & e->type);
        }
    }

    if (id_to_erase != -1) {
        ids.erase(ids.begin() + id_to_erase);

        if (entityInherits(e->type, MESH_ENTITY)) {
            auto pos = ((MeshEntity*)e)->position + ((MeshEntity*)e)->gizmo_position_offset;
            avg_position = (float)avg_position_count * avg_position - pos;
            avg_position_count--;
            avg_position /= (float)avg_position_count;
        }
        return;
    }
    else {
        ids.push_back(e->id);
        if (type == ENTITY) type = e->type;
        else                type = (EntityType)(type & e->type);

        if (entityInherits(e->type, MESH_ENTITY)) {
            auto pos = ((MeshEntity*)e)->position + ((MeshEntity*)e)->gizmo_position_offset;
            avg_position = (float)avg_position_count * avg_position + pos;
            avg_position_count++;
            avg_position /= (float)avg_position_count;
        }
    }
}
void ReferenceSelection::clear() {
    ids.clear();
    type = ENTITY;
    avg_position_count = 0;
    avg_position = glm::vec3(0.0);
    submesh_i = -1;
}

CopySelection::~CopySelection() {
    for (auto& entity : entities) {
        free(entity);
    }
    entities.clear();
}
void CopySelection::free_clear() {
    for (auto& entity : entities) {
        free(entity);
    }
    entities.clear();
}

void referenceToCopySelection(EntityManager& entity_manager, const ReferenceSelection& ref, CopySelection& cpy) {
    cpy.free_clear();
    for (const auto& id : ref.ids) {
        auto e = entity_manager.getEntity(id);
        if (e == nullptr) continue;

        auto cpy_e = copyEntity(e);
        if (entityInherits(cpy_e->type, MESH_ENTITY)) {
            auto m_e = (MeshEntity*)cpy_e;
            
            if (m_e->mesh != nullptr) {
                m_e->mesh = new Mesh();
                m_e->mesh->handle = ((MeshEntity*)e)->mesh->handle;
            }
        }
        if (entityInherits(cpy_e->type, ANIMATED_MESH_ENTITY)) {
            auto a_e = (AnimatedMeshEntity*)cpy_e;

            if (a_e->animesh != nullptr) {
                a_e->animesh = new AnimatedMesh();
                a_e->animesh->handle = ((AnimatedMeshEntity*)e)->animesh->handle;
            }
        }

        cpy.entities.emplace_back(std::move(cpy_e));
    }
}

// @note doesn't preserve relationships between entities
void createCopySelectionEntities(EntityManager& entity_manager, AssetManager &asset_manager, CopySelection& cpy, ReferenceSelection &ref) {
    for (auto& e : cpy.entities) {
        if (entity_manager.water != NULLID && entityInherits(e->type, WATER_ENTITY)) {
            std::cerr << "Warning, attempted to copy water to entity_manager which already has one. Skipping\n";
            continue;
        }
        auto id = entity_manager.getFreeId();
        entity_manager.setEntity(id.i, e);
        if (entityInherits(e->type, WATER_ENTITY)) entity_manager.water = e->id;
        ref.addEntity(e);

        // @note this needs to makes sure all assets are correctly copied,
        // @todo systematise copying entities and assets, maybe only store path? (might be slow)
        if (entityInherits(e->type, MESH_ENTITY)) {
            auto m_e = (MeshEntity*)e;
            if (m_e->mesh == nullptr) continue;
            
            auto path = m_e->mesh->handle;
            free(m_e->mesh);

            auto mesh = asset_manager.getMesh(path);
            if (mesh == nullptr) {
                mesh = asset_manager.createMesh(path);
                asset_manager.loadMeshFile(mesh, path);
            }
            m_e->mesh = mesh;
            m_e->position += translation_snap;
        }
        if (entityInherits(e->type, ANIMATED_MESH_ENTITY)) {
            auto m_e = (AnimatedMeshEntity*)e;
            if (m_e->animesh == nullptr) continue;

            auto path = m_e->animesh->handle;
            free(m_e->animesh);

            auto animesh = asset_manager.getAnimatedMesh(path);
            if (animesh == nullptr) {
                animesh = asset_manager.createAnimatedMesh(path);
                asset_manager.loadAnimationFile(animesh, path);
            }
            m_e->animesh = animesh;
        }
    }
    // memory is now owned by entity manager so makes sure we clear cpy
    cpy.entities.clear();
}

void initEditorGui(AssetManager &asset_manager){
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    
    // create a file browser instance
    im_file_dialog_type = "";

    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.19f, 0.19f, 0.19f, 0.92f);
    colors[ImGuiCol_Border] = ImVec4(0.19f, 0.19f, 0.19f, 0.29f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.54f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
    colors[ImGuiCol_Button] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.22f, 0.23f, 0.33f);
    colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 0.36f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
    colors[ImGuiCol_NavHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.35f);

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(8.00f, 8.00f);
    style.FramePadding = ImVec2(5.00f, 4.00f);
    style.CellPadding = ImVec2(6.00f, 6.00f);
    style.ItemSpacing = ImVec2(6.00f, 6.00f);
    style.ItemInnerSpacing = ImVec2(6.00f, 6.00f);
    style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
    style.IndentSpacing = 25;
    style.ScrollbarSize = 15;
    style.GrabMinSize = 10;
    style.WindowBorderSize = 1;
    style.ChildBorderSize = 1;
    style.PopupBorderSize = 1;
    style.FrameBorderSize = 1;
    style.TabBorderSize = 1;
    style.WindowRounding = 7;
    style.ChildRounding = 4;
    style.FrameRounding = 3;
    style.PopupRounding = 4;
    style.ScrollbarRounding = 9;
    style.GrabRounding = 3;
    style.LogSliderDeadzone = 4;
    style.TabRounding = 4;
    style.Alpha = 0.9;

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(gl_state.glsl_version.c_str());

    asset_manager.loadMeshFile(&arrow_mesh, "data/mesh/arrow.mesh");
    asset_manager.loadMeshFile(&block_arrow_mesh, "data/mesh/block_arrow.mesh");
    asset_manager.loadMeshFile(&ring_mesh, "data/mesh/ring.mesh");

    entity_type_to_string[ENTITY] = "Basic Entity";
    entity_type_to_string[MESH_ENTITY] = "Mesh";
    entity_type_to_string[WATER_ENTITY] = "Water";
    entity_type_to_string[COLLIDER_ENTITY] = "Mesh Collider";
    entity_type_to_string[ANIMATED_MESH_ENTITY] = "Animated Mesh";
    entity_type_to_string[PLAYER_ENTITY] = "Player";
}

static bool runLightmapperCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager) {
    runLightmapper(loaded_level, asset_manager);

    return true;
}

static bool echoCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager) {
    for (int i = 1; i < input_tokens.size(); ++i) {
        if (i != 1) output.append(" ");
        output.append(input_tokens[i]);
    }
    return true;
}

static bool listLevelsCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager) {
    for (const auto& entry : std::filesystem::directory_iterator("data/levels/")) {
        auto filename = entry.path().filename();
        if (filename.extension() == ".level") {
            std::string name = filename.stem().generic_string();
            output += name + "   ";
        }
    }
    return true;
}

static bool listMeshCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager) {
    for (const auto& entry : std::filesystem::directory_iterator("data/mesh/")) {
        auto filename = entry.path().filename();
        if (filename.extension() == ".mesh") {
            std::string name = filename.stem().generic_string();
            output += name + "   ";
        }
    }
    return true;
}

static bool loadLevelCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager) {
    if (input_tokens.size() >= 2) {
        auto filename = "data/levels/" + input_tokens[1] + ".level";
        if (loadLevel(loaded_level, asset_manager, filename)) {
            if (gamestate.is_active) {
                gamestate.initialized = false;
                playGame();
            }

            output += "Loaded level at path " + filename;
            selection.clear();
            Cameras::update_cameras_for_level();
            return true;
        }

        output += "Failed to loaded level at path " + filename;
        return false;
    }
    
    output += "Please provide a level name, see list_levels";
    return false;
}

static bool clearLevelCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager) {
    loaded_level.entities.clear();
    selection.clear();
    output += "Cleared level";

    return true;
}

static bool saveLevelCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager) {
    std::string filename;
    if (input_tokens.size() == 1) {
        if (loaded_level.path == "") {
            output += "Current level path not set";
            return false;
        }
        filename = loaded_level.path;
    }
    else if (input_tokens.size() >= 2) {
        filename = "data/levels/" + input_tokens[1] + ".level";
    }
    saveLevel(loaded_level, filename);

    output += "Saved current level at path " + filename;
    return true;
}

static bool newLevelCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager) {
    std::string filename;
    if (input_tokens.size() == 1) {
        output += "Please provide a name for the new level";
        return false;
    }

    std::string dummy;
    clearLevelCommand(input_tokens, dummy, entity_manager, asset_manager);
    loaded_level.path = "data/levels/" + input_tokens[1] + ".level";
    
    output += "Created new level at with path " + filename;
    return true;
}

static bool loadModelCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager) {
    if (input_tokens.size() >= 3) {
        auto model_filename = input_tokens[1];
        auto mesh_filename = "data/mesh/" + input_tokens[2] + ".mesh";
        auto mesh = asset_manager.createMesh(mesh_filename);
        
        if (asset_manager.loadMeshAssimp(mesh, model_filename)) {
            output += "Loaded model " + model_filename + "\n";
            if (asset_manager.writeMeshFile(mesh, mesh_filename)) {
                output += "Wrote mesh file at path " + mesh_filename + "\n";
                return true;
            }

            output += "Failed to write mesh file at path " + mesh_filename + "\n";
        }
        return false;
    }
    
    output += "Please provide a relative path to a model (obj, gltf, fbx, ...) and a name for the new mesh";
    return false;
}

static bool loadAnimatedModelCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager) {
    if (input_tokens.size() >= 3) {
        auto model_filename = input_tokens[1];
        auto mesh_filename = "data/mesh/" + input_tokens[2] + ".mesh";
        auto anim_filename = "data/anim/" + input_tokens[2] + ".anim";
        auto mesh = asset_manager.createMesh(mesh_filename);
        auto anim = asset_manager.createAnimatedMesh(anim_filename);

        if (asset_manager.loadAnimatedMeshAssimp(anim, mesh, model_filename)) {
            output += "Loaded animated model " + model_filename + "\n";
            if (asset_manager.writeMeshFile(mesh, mesh_filename)) {
                output += "Wrote mesh file to " + mesh_filename + "\n";
            }
            else {
                output += "Failed to write mesh file to " + mesh_filename + "\n";
                return false;
            }
            if (asset_manager.writeAnimationFile(anim, anim_filename)) {
                output += "Wrote mesh file to " + anim_filename + "\n";
            }
            else {
                output += "Failed to write anim file to " + anim_filename + "\n";
                return false;
            }

            return true;
        }
        return false;
    }

    output += "Please provide a relative path to a model (obj, gltf, fbx, ...) and a name for the new animated mesh";
    return false;
}

static bool addMeshCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager) {
    if (input_tokens.size() >= 2) {
        auto filename = "data/mesh/" + input_tokens[1] + ".mesh";
        auto mesh = asset_manager.getMesh(filename);

        if (mesh == nullptr) {
            mesh = asset_manager.createMesh(filename);
            if (asset_manager.loadMeshFile(mesh, filename)) {
                output += "Loaded mesh at path " + filename + "\n";
            }
            else {
                output += "Failed to loaded mesh at path " + filename + " maybe it doesn't exist\n";
                return false;
            }
        }
        else {
            output += "Mesh has already been loaded, using program memory\n";
        }

        auto new_mesh_entity = (MeshEntity*)entity_manager.createEntity(MESH_ENTITY);
        new_mesh_entity->mesh = mesh;
        new_mesh_entity->casts_shadow = true;
        selection.addEntity(new_mesh_entity);

        output += "Added mesh entity with provided mesh to level\n";
        return true;
    }

    output += "Please provide a mesh name, see list_mesh\n";
    return false;
}

static bool addAnimatedMeshCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager) {
    if (input_tokens.size() >= 2) {
        auto mesh_filename = "data/mesh/" + input_tokens[1] + ".mesh";
        auto anim_filename = "data/anim/" + input_tokens[1] + ".anim";
        auto mesh = asset_manager.getMesh(mesh_filename);
        auto anim = asset_manager.getAnimatedMesh(anim_filename);

        if (mesh == nullptr) {
            mesh = asset_manager.createMesh(mesh_filename);
            if (asset_manager.loadMeshFile(mesh, mesh_filename)) {
                output += "Loaded mesh at path " + mesh_filename + "\n";
            }
            else {
                output += "Failed to loaded mesh at path " + mesh_filename + " maybe it doesn't exist\n";
                return false;
            }
        }
        if (anim == nullptr) {
            anim = asset_manager.createAnimatedMesh(anim_filename);
            if (asset_manager.loadAnimationFile(anim, anim_filename)) {
                output += "Loaded anim at path " + anim_filename + "\n";
            }
            else {
                output += "Failed to loaded anim at path " + anim_filename + " maybe it doesn't exist\n";
                return false;
            }
        }

        auto new_animesh_entity = (AnimatedMeshEntity*)entity_manager.createEntity(ANIMATED_MESH_ENTITY);
        new_animesh_entity->mesh = mesh;
        new_animesh_entity->animesh = anim;
        new_animesh_entity->casts_shadow = true;
        selection.addEntity(new_animesh_entity);

        output += "Added animated mesh entity with provided mesh to level\n";
        return true;
    }

    output += "Please provide a mesh name, see list_mesh\n";
    return false;
}

static bool addColliderCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager) {
    auto collider = new ColliderEntity(entity_manager.getFreeId());
    auto collider_mesh = asset_manager.getMesh("data/mesh/cube.mesh");
    if (collider_mesh == nullptr) {
        collider->mesh = asset_manager.createMesh("data/mesh/cube.mesh");
        if (!asset_manager.loadMeshFile(collider->mesh, "data/mesh/cube.mesh")) {
            output += "Failed to load collider mesh";
            return false;
        }
    }
    else {
        collider->mesh = collider_mesh;
    }
    entity_manager.setEntity(collider->id.i, collider);
    
    return true;
}

static bool saveMeshCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager) {
    if (input_tokens.size() >= 2) {
        auto filename = "data/mesh/" + input_tokens[1] + ".mesh";
        auto mesh = asset_manager.getMesh(filename);

        if (mesh != nullptr) {
            if (asset_manager.writeMeshFile(mesh, filename)) {
                output += "Wrote mesh file to path " + filename + "\n";
                return true;
            }
            
            output += "Failed to write mesh file to path " + filename + "\n";
            return false;
        }

        output += "A Mesh with handle '" + filename + "' hasn't been loaded\n";
        return false;
    }

    output += "Please provide a mesh name, see list_mesh\n";
    return false;
}

static bool listModelsCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager) {
    for (const auto& entry : std::filesystem::directory_iterator("data/models/")) {
        auto filename = entry.path().filename();
        if (filename.extension() == ".obj") {
            std::string name = filename.stem().generic_string();
            output += name + "   ";
        }
    }
    return true;
}

static bool convertModelsToMeshCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager) {
    for (const auto& entry : std::filesystem::directory_iterator("data/models/")) {
        auto filename = entry.path().filename();
        if (filename.extension() == ".obj") {
            auto model_filename = "data/models/" + filename.generic_string();
            auto mesh_filename  = "data/mesh/"   + filename.stem().generic_string() + ".mesh";
            auto mesh = asset_manager.createMesh(mesh_filename);

            if (asset_manager.loadMeshAssimp(mesh, model_filename)) {
                output += "Loaded model " + model_filename + "\n";
                if (asset_manager.writeMeshFile(mesh, mesh_filename)) {
                    output += "Wrote mesh file at path " + mesh_filename + "\n";
                }
                else {
                    output += "Failed to write mesh file at path " + mesh_filename + "\n";
                }
            }
        }
    }
    return true;
}

static bool addWaterCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager) {
    if(entity_manager.water == NULLID) {
        auto w = (WaterEntity*)entity_manager.createEntity(WATER_ENTITY);
        entity_manager.water = w->id;
        selection.addEntity(w);
        return true;
    }
    
    output += "Failed to create water, there already is one in level\n";
    return false;
}

static bool toggleBloomCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager) {
    graphics::do_bloom = !graphics::do_bloom;
    if (graphics::do_bloom) {
        initBloomFbo(true);
        initHdrFbo(true);
        output += "Enabled bloom\n";
    }
    else {
        initHdrFbo(true);
        output += "Disabled bloom\n";
    }
    return true;
}

static bool toggleVolumetricsCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager) {
    graphics::do_volumetrics = !graphics::do_volumetrics;
    if (graphics::do_volumetrics) {
        output += "Enabled volumetrics\n";
    }
    else {
        output += "Disabled volumetrics\n";
    }
    return true;
}

static bool toggleShadowsCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager) {
    graphics::do_shadows = !graphics::do_shadows;
    if (graphics::do_shadows) {
        output += "Enabled shadows\n";
    }
    else {
        output += "Disabled shadows\n";
    }
    return true;
}

static bool setMsaaCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager) {
    if (input_tokens.size() >= 2) {
        int msaa_samples = std::stoi(input_tokens[1]);
        if (msaa_samples <= 0) {
            graphics::do_msaa = false;
            output += "Disabled MSAA\n";
        }
        else {
            graphics::do_msaa = true;
            graphics::MSAA_SAMPLES = msaa_samples;
            output += "Enabled MSAA with " + std::to_string(msaa_samples) + " samples\n";
        }

        initHdrFbo(true);
        return true;
    }
    output += "Please specify the number of samples, eg. 4 or 0 (no MSAA)";
    return false;
}

static bool helpCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager);

const std::map
<
    std::string,
    std::function<bool(std::vector<std::string> &, std::string &output, EntityManager&, AssetManager&)>
> command_to_func = {
    {"echo", echoCommand},
    {"lightmap", runLightmapperCommand},
    {"help", helpCommand},
    {"list_levels", listLevelsCommand},
    {"load_level", loadLevelCommand},
    {"clear_level", clearLevelCommand},
    {"new_level", newLevelCommand},
    {"list_mesh", listMeshCommand},
    {"add_mesh", addMeshCommand},
    {"add_animated_mesh", addAnimatedMeshCommand},
    {"add_collider", addColliderCommand},
    {"load_model", loadModelCommand},
    {"load_animated_model", loadAnimatedModelCommand},
    {"list_models", listModelsCommand},
    {"save_level", saveLevelCommand},
    {"convert_models_to_mesh", convertModelsToMeshCommand},
    {"add_water", addWaterCommand},
    {"toggle_bloom", toggleBloomCommand},
    {"toggle_volumetrics", toggleVolumetricsCommand},
    {"toggle_shadows", toggleShadowsCommand},
    {"set_msaa", setMsaaCommand},
};

static bool helpCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager) {
    output += "Commands are: clear ";
    for (const auto& pair : command_to_func) {
        output += pair.first + " ";
    }
    return true;
}

void ImTerminal(EntityManager &entity_manager, AssetManager &asset_manager, bool is_active) {
    constexpr int pad = 10;
    constexpr float open_time_total = 0.6; // s
    constexpr float close_time_total = 0.3; // s
    const float height = std::min(window_height / 2.5f, 250.f);

    static bool prev_is_active = false;
    static float height_offset = 0;
    static float change_time = glfwGetTime();
    if(prev_is_active != is_active) change_time = glfwGetTime();
    
    bool is_opening = is_active && (height_offset != height);
    bool is_closing = !is_active && (height_offset != 0);

    if(is_opening) {
        float t = glm::smoothstep(0.0f, open_time_total, (float)glfwGetTime() - change_time);
        height_offset = glm::mix(height, height_offset, 1-t);
    } else if (is_closing) {
        float t = glm::smoothstep(0.0f, close_time_total, (float)glfwGetTime() - change_time);
        height_offset = glm::mix(height_offset, 0.0f, t);
    }

    static std::vector<std::string> command_history = {};
    static std::vector<std::string> result_history = {};

    static bool update_input_contents = false;
    static int history_position = -1;
    static std::string saved_input_line{""};
    static std::string input_line{""};
    const char prompt[] = "> ";
    if(is_active || is_closing) {
        ImGui::SetNextWindowPos(ImVec2(0, window_height - height_offset));
        ImGui::SetNextWindowSize(ImVec2(window_width, height));

        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0,0,0,200));
        ImGui::Begin("Terminal", NULL, ImGuiWindowFlags_NoDecoration);
        ImGui::PopStyleColor();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,5));

        auto len = std::min(command_history.size(), result_history.size());
        for(int i = 0; i < len; ++i) {
            if(command_history[i] != "") ImGui::TextWrapped("%s%s\n", prompt, command_history[i].c_str());
            if(result_history[i] != "")  ImGui::TextWrapped("%s", result_history[i].c_str());
        }

        ImGui::Text(prompt);
        ImGui::SameLine();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0,0));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0,0,0,0));

        static bool edit_made = false;
        static bool up_press = false;
        static bool down_press = false;
        static std::string suggestion_out = "";
        struct Callbacks { 
            static int callback(ImGuiInputTextCallbackData* data) { 
                if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
                    if (data->EventKey == ImGuiKey_UpArrow) {
                        up_press = true;
                    } else if (data->EventKey == ImGuiKey_DownArrow) {
                        down_press = true;
                    }
                } else if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
                    edit_made = true;
                    if (data->EventChar == '`') {
                        return 1;
                    }

                }
                else if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
                    auto last_space = input_line.find(" ");
                    if (last_space == std::string::npos) {
                        last_space = 0;
                    }
                    else {
                        last_space++;
                    }

                    // Only do completion if we are at the first word
                    // @todo command completion
                    if (input_line.find(" ", last_space) == std::string::npos) {
                        auto word = input_line.substr(last_space, std::string::npos);

                        std::vector<std::string> suggestions;
                        for (auto& d : command_to_func) {
                            auto& keywrd = d.first;

                            // Kind of fuzzy search and easy
                            if (keywrd.find(word) != std::string::npos) {
                                suggestions.emplace_back(keywrd);
                            }
                        }

                        suggestion_out = "";
                        if (suggestions.size() == 1) {
                            input_line = input_line.substr(0, last_space) + suggestions[0];
                            update_input_contents = true;
                        }
                        else {
                            for (auto& s : suggestions) {
                                suggestion_out += "\t" + s;
                            }
                        }
                    }
                }
                else if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit) {
                    suggestion_out = "";
                }

                if(update_input_contents) {
                    data->DeleteChars(0, data->BufTextLen);
                    data->InsertChars(0, input_line.c_str());
                    update_input_contents = false;
                }
                return 0; 
            } 
        };
        ImGui::SetNextItemWidth(window_width);
        ImGui::SetKeyboardFocusHere(0);
        auto flags = ImGuiInputTextFlags_NoHorizontalScroll | ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackEdit;
        bool enter_pressed = ImGui::InputTextWithHint("###input_line", "Enter Command", &input_line, flags, Callbacks::callback);

        // Add tab suggestions
        if (suggestion_out != "")  ImGui::TextWrapped("%s", suggestion_out.c_str());

        if(enter_pressed) {
            suggestion_out = "";

            auto &output = result_history.emplace_back ("");

            // Hack to make empty lines visible
            if(input_line == "") {
                command_history.emplace_back(" ");
            } else {
                auto &input  = command_history.emplace_back(input_line);

                auto tokens = split(input_line, " ");
                if(tokens.size() > 0) {
                    auto &command = tokens[0];

                    // Clear is a special case as it modifies history
                    if (command == "clear") {
                        command_history.clear();
                        result_history.clear();
                    } else {
                        auto lu = command_to_func.find(command);
                        if (lu != command_to_func.end()) {
                            lu->second(tokens, output, entity_manager, asset_manager);
                        }
                        else {
                            output = "Unknown Command '" + tokens[0] + "', try 'help' to see a list of commands";
                        }
                    }
                }

            }
            input_line = "";
        }

        if(edit_made || enter_pressed) {
            history_position = -1;
        }
        if(command_history.size() > 0) {
            if(up_press && history_position < (int)command_history.size() - 1) {
                if(history_position == -1) {
                    saved_input_line = input_line;
                    history_position = 0;
                } else {
                    history_position++;
                }
                input_line = command_history[(int)command_history.size() - history_position - 1];
                update_input_contents = true;
            } else if(down_press) {
                if(history_position == 0) {
                    history_position = -1;
                    input_line = saved_input_line;
                    update_input_contents = true;
                } else if(history_position > 0) {
                    history_position--;
                    input_line = command_history[(int)command_history.size() - history_position - 1];
                    update_input_contents = true;
                }
            }
        }

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor();

        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        if (edit_made)
            ImGui::SetScrollY(ImGui::GetScrollMaxY());

        ImGui::End();

        up_press = false;
        down_press = false;
        edit_made = false;

        // Extra scrolling is happening in editor 
        // @fix?
        Controls::scroll_offset = glm::vec2(0);
    }

    prev_is_active = is_active;
}

TransformType editTransform(Camera &camera, glm::vec3 &pos, glm::quat &rot, glm::mat3 &scl, TransformType type=TransformType::ALL){
    static bool use_trans_snap = true, use_rot_snap = true, use_scl_snap = false;
    static glm::vec3 scale_snap = glm::vec3(1.0);
    static float rotation_snap = 90;
    TransformType change_type = TransformType::NONE;

    bool do_p = (bool)((unsigned int)type & (unsigned int)TransformType::POS);
    bool do_r = (bool)((unsigned int)type & (unsigned int)TransformType::ROT);
    bool do_s = (bool)((unsigned int)type & (unsigned int)TransformType::SCL);

    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureKeyboard && camera.state != Camera::Type::SHOOTER) {
        if (Controls::editor.isAction("toggle_translate") && do_p)
            gizmo_mode = gizmo_mode == GizmoMode::TRANSLATE ? GizmoMode::NONE : GizmoMode::TRANSLATE;
        if (Controls::editor.isAction("toggle_rotate") && do_r)
            gizmo_mode = gizmo_mode == GizmoMode::ROTATE ? GizmoMode::NONE : GizmoMode::ROTATE;
        if (Controls::editor.isAction("toggle_scale") && do_s)
            gizmo_mode = gizmo_mode == GizmoMode::SCALE ? GizmoMode::NONE : GizmoMode::SCALE;
    }

    if(do_p){
        if (ImGui::RadioButton("##translate", gizmo_mode == GizmoMode::TRANSLATE)){
            gizmo_mode = gizmo_mode == GizmoMode::TRANSLATE ? GizmoMode::NONE : GizmoMode::TRANSLATE;
        }  
        ImGui::SameLine();
        ImGui::TextWrapped("Translation (Vector 3)");
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 10);
        if(ImGui::InputFloat3("##translation", &pos[0])){
            change_type = (TransformType)((uint64_t)change_type | (uint64_t)TransformType::POS);
        } 
    }
    if(do_r){
        if (ImGui::RadioButton("##rotate", gizmo_mode == GizmoMode::ROTATE)){
            gizmo_mode = gizmo_mode == GizmoMode::ROTATE ? GizmoMode::NONE : GizmoMode::ROTATE;
        }
        ImGui::SameLine();
        ImGui::TextWrapped("Rotation (Quaternion)");
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 10);
        if(ImGui::InputFloat4("##rotation", &rot[0])){
            change_type = (TransformType)((uint64_t)change_type | (uint64_t)TransformType::ROT);
        } 
    }
    if(do_s){
        if (ImGui::RadioButton("##scale", gizmo_mode == GizmoMode::SCALE)){
            gizmo_mode = gizmo_mode == GizmoMode::SCALE ? GizmoMode::NONE : GizmoMode::SCALE;
        }
        ImGui::SameLine();
        ImGui::TextWrapped("Scale (Vector 3)");
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 10);
        glm::vec3 scale = glm::vec3(scl[0][0], scl[1][1], scl[2][2]);
        if(ImGui::InputFloat3("##scale", &scale[0])){
            scl[0][0] = scale.x;
            scl[1][1] = scale.y;
            scl[2][2] = scale.z;
            change_type = (TransformType)((uint64_t)change_type | (uint64_t)TransformType::SCL);
        } 
    }
  
    switch (gizmo_mode)
    {
    case GizmoMode::TRANSLATE:
        if (!io.WantCaptureKeyboard && Controls::editor.isAction("toggle_snap"))
            use_trans_snap = !use_trans_snap;
        ImGui::Checkbox("", &use_trans_snap);
        ImGui::SameLine();
        ImGui::InputFloat3("Snap", &translation_snap[0]);
        if(do_p && editorTranslationGizmo(pos, rot, scl, camera, translation_snap, use_trans_snap))
            change_type = (TransformType)((uint64_t)change_type | (uint64_t)TransformType::POS);
        break;
    case GizmoMode::ROTATE:
        if (!io.WantCaptureKeyboard && Controls::editor.isAction("toggle_snap"))
            use_rot_snap = !use_rot_snap;
        ImGui::Checkbox("", &use_rot_snap);
        ImGui::SameLine();
        ImGui::InputFloat("Snap", &rotation_snap);
        if(do_r && editorRotationGizmo(pos, rot, scl, camera, (rotation_snap / 180.f) * PI, use_rot_snap))
            change_type = (TransformType)((uint64_t)change_type | (uint64_t)TransformType::ROT);
        break;
    case GizmoMode::SCALE:
        if (!io.WantCaptureKeyboard && Controls::editor.isAction("toggle_snap"))
            use_scl_snap = !use_scl_snap;
        ImGui::Checkbox("", &use_scl_snap);
        ImGui::SameLine();
        ImGui::InputFloat3("Snap", &scale_snap[0]);
        if(do_s && editorScalingGizmo(pos, rot, scl, camera, scale_snap, use_scl_snap))
            change_type = (TransformType)((uint64_t)change_type | (uint64_t)TransformType::SCL);
        break;
    default:
        break;
    }

    return change_type;
}

bool raycastGizmoMeshAxis(Raycast& raycast, Mesh& mesh, glm::mat3 axis, glm::mat3 positions, glm::mat3 scales) {
    auto tmp_raycast = raycast;
    for (int i = 0; i < 3; ++i) {
        const auto direction = axis[i];

        static const auto mesh_up = glm::vec3(0, 1, 0); // The up direction of the mesh should be aligned with y axis
        auto transform = createModelMatrix(positions[i], glm::quat(direction, mesh_up), scales[i]);

        if (raycastTriangles(mesh.vertices, mesh.indices, mesh.num_indices, transform, tmp_raycast)) {
            if (tmp_raycast.result.t < raycast.result.t) {
                if (raycastPlane(positions[i], direction, tmp_raycast)) {
                    raycast = tmp_raycast;
                    raycast.result.indice = i;
                }
            }
        }
    }
    return raycast.result.hit;
}

bool editorRotationGizmo(glm::vec3& pos, glm::quat& rot, glm::mat3& scl, const Camera& camera, float snap = 1.0, bool do_snap = false) {
    static int selected_ring = -1;
    static glm::vec3 rot_dir_prev;

    const glm::mat3 axis = {
        glm::vec3(-1,0,0),
        glm::vec3(0,1,0),
        glm::vec3(0,0,-1)
    };
    static const glm::mat3 scales = {
        glm::vec3(0.1,  0.1, 0.1),
        glm::vec3(0.09, 0.1, 0.09),
        glm::vec3(0.08, 0.1, 0.08),
    };
    glm::mat3 positions { pos, pos, pos };
  
    auto raycast = mouseToRaycast(Controls::mouse_position, glm::ivec2(window_width, window_height), camera.inv_vp);
    const float distance = glm::length(pos - camera.position);

    bool initial_selection = false;
    if(Controls::editor.left_mouse.state == Controls::PRESS) {
        if (raycastGizmoMeshAxis(raycast, ring_mesh, axis, positions, distance * scales)) {
            selected_ring = raycast.result.indice;
            initial_selection = true;
        }
    } else if (Controls::editor.left_mouse.state & Controls::RELEASED) {
        selected_ring = -1;
    }

    // @todo add debug meshes to render queue for transparent objects and do proper sorting
    gl_state.set_flags(GlFlags::DEPTH_WRITE | GlFlags::BLEND | GlFlags::CULL);
    glClear(GL_DEPTH_BUFFER_BIT);
    
    const bool active = selected_ring != -1;
    if (active){
        const auto normal = axis[selected_ring];

        // Calculate which direction on the gizmo the user has moved to
        if (raycastPlane(pos, normal, raycast)) {
            glm::vec3 rot_dir = glm::normalize(raycast.origin + raycast.result.t * raycast.direction - pos);
            if (initial_selection)
                rot_dir_prev = rot_dir;

            const glm::vec3 arrow_size = distance * glm::vec3(0.02, 0.3 * scales[selected_ring].x, 0.02);
            drawEditor3DArrow(pos, rot_dir*glm::vec3(-1,1,-1), camera, glm::vec4(1.0), arrow_size, false);

            // Apply a rotation to move to the grabbed position
            float angle = angleBetweenDirections(rot_dir_prev, rot_dir, normal);
            if (do_snap) {
                // Show a more transparent arrow which shows the start rotation, this helps show that rotation is working but snapped
                drawEditor3DArrow(pos, rot_dir_prev*glm::vec3(-1,1,-1), camera, glm::vec4(0.5), arrow_size, false);

                if (angle >= snap || angle <= -snap) {
                    angle = glm::floor(angle / snap) * snap;
                    rot = glm::rotate(rot, angle, glm::inverse(rot)*normal);
                    rot_dir_prev = rot_dir;
                }
            } else {
                rot = glm::rotate(rot, angle, glm::inverse(rot)*normal);
                rot_dir_prev = rot_dir;
            }
        }
    }


    for (int i = 0; i < 3; ++i) {
        const glm::vec4 color(i==0, i==1, i==2, 1.0 - 0.5*(selected_ring != i));
        drawEditor3DRing(positions[i], axis[i], camera, color, distance*scales[i], false);
    }

    return active;
}

bool editorTranslationGizmo(glm::vec3 &pos, glm::quat &rot, glm::mat3 &scl, Camera &camera, const glm::vec3 &snap, bool do_snap){
    static int selected_arrow = -1;
    static glm::vec3 selection_offset;
    
    const glm::mat3 axis = {
        glm::vec3(-1,0,0),
        glm::vec3(0,1,0),
        glm::vec3(0,0,-1)
    };
    static const glm::mat3 scales = {
        glm::vec3(0.03, 0.05, 0.03),
        glm::vec3(0.03, 0.05, 0.03),
        glm::vec3(0.03, 0.05, 0.03),
    };
    glm::mat3 positions{ pos, pos, pos };

    auto raycast = mouseToRaycast(Controls::mouse_position, glm::ivec2(window_width, window_height), camera.inv_vp);
    const float distance = glm::length(pos - camera.position);
    
    bool initial_selection = false;
    if (Controls::editor.left_mouse.state == Controls::PRESS) {
        if (raycastGizmoMeshAxis(raycast, arrow_mesh, axis, positions, distance * scales * 1.1f)) {
            selected_arrow = raycast.result.indice;
            selection_offset = glm::normalize(raycast.origin + raycast.result.t * raycast.direction - pos);
            initial_selection = true;
        }
    }
    else if (Controls::editor.left_mouse.state & Controls::RELEASED) {
        selected_arrow = -1;
    }

    
    const bool active = selected_arrow != -1;

    if(active){
        auto arrow_dir = axis[selected_arrow];
        auto arrow_pos = positions[selected_arrow];
        static float initial_t;
        
        float raycast_t, arrow_t;
        distanceBetweenLines(raycast.origin, raycast.direction, arrow_pos + selection_offset, arrow_dir, raycast_t, arrow_t);
        if (initial_selection)
            initial_t = arrow_t;
        auto trans = -(arrow_t - initial_t)*arrow_dir;

        if(do_snap){
            trans.x = glm::round(trans.x / snap.x) * snap.x;
            trans.y = glm::round(trans.y / snap.y) * snap.y;
            trans.z = glm::round(trans.z / snap.z) * snap.z;
        }
        pos += trans;
    }

    // @todo add debug meshes to render queue for transparent objects and do proper sorting
    gl_state.set_flags(GlFlags::DEPTH_WRITE | GlFlags::BLEND | GlFlags::CULL);
    glClear(GL_DEPTH_BUFFER_BIT);

    for (int i = 0; i < 3; ++i) {
        const glm::vec4 color(i == 0, i == 1, i == 2, 1.0 - 0.5 * (selected_arrow != i));
        drawEditor3DArrow(positions[i], axis[i], camera, color, distance * scales[i], false);
    }

    return active;
}

bool editorScalingGizmo(glm::vec3 &pos, glm::quat &rot, glm::mat3 &scl, Camera &camera, const glm::vec3 &snap, bool do_snap){
    static int selected_arrow = -1;
    static glm::vec3 selection_offset;

    const glm::mat3 axis = {
        glm::vec3(-1,0,0),
        glm::vec3(0,1,0),
        glm::vec3(0,0,-1)
    };
    static const glm::mat3 scales = {
        glm::vec3(0.04, 0.04, 0.04),
        glm::vec3(0.04, 0.04, 0.04),
        glm::vec3(0.04, 0.04, 0.04),
    };
    glm::mat3 positions{ pos, pos, pos };

    auto raycast = mouseToRaycast(Controls::mouse_position, glm::ivec2(window_width, window_height), camera.inv_vp);
    const float distance = glm::length(pos - camera.position);

    bool initial_selection = false;
    if (Controls::editor.left_mouse.state == Controls::PRESS) {
        if (raycastGizmoMeshAxis(raycast, block_arrow_mesh, axis, positions, distance * scales * 1.1f)) {
            selected_arrow = raycast.result.indice;
            selection_offset = glm::normalize(raycast.origin + raycast.result.t * raycast.direction - pos);
            initial_selection = true;
        }
    }
    else if (Controls::editor.left_mouse.state & Controls::RELEASED) {
        selected_arrow = -1;
    }

    const bool active = selected_arrow != -1;
    if(active){
        static float prev_t;
        auto arrow_dir = axis[selected_arrow];
        auto arrow_pos = positions[selected_arrow];

        float raycast_t, arrow_t;
        distanceBetweenLines(raycast.origin, raycast.direction, arrow_pos + selection_offset, arrow_dir, raycast_t, arrow_t);

        if (initial_selection)
            prev_t = arrow_t;
        auto scale = -(arrow_t - prev_t)*arrow_dir;
        prev_t = arrow_t;

        if(do_snap) {
            scale.x = glm::round(scale.x / snap.x) * snap.x;
            scale.y = glm::round(scale.y / snap.y) * snap.y;
            scale.z = glm::round(scale.z / snap.z) * snap.z;
        }
        scl[0][0] += scale.x;
        scl[1][1] += scale.y;
        scl[2][2] += scale.z;
    }

    // @todo add debug meshes to render queue for transparent objects and do proper sorting
    gl_state.set_flags(GlFlags::DEPTH_WRITE | GlFlags::BLEND | GlFlags::CULL);
    glClear(GL_DEPTH_BUFFER_BIT);

    for (int i = 0; i < 3; ++i) {
        const glm::vec4 color(i == 0, i == 1, i == 2, 1.0 - 0.5 * (selected_arrow != i));
        drawEditor3DArrow(positions[i], axis[i], camera, color, distance * scales[i], false, true);
    }

    return active;
}


void drawWaterDebug(WaterEntity* w, const Camera &camera, bool flash = false) {
    bindBackbuffer();
    gl_state.set_flags(GlFlags::CULL | GlFlags::DEPTH_READ);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glEnable(GL_LINE_SMOOTH);
    glLineWidth(1.0);

    gl_state.bind_program(Shaders::debug.program());
    auto mvp = camera.vp * createModelMatrix(w->position, glm::quat(), w->scale);
    glUniformMatrix4fv(Shaders::debug.uniform("mvp"), 1, GL_FALSE, &mvp[0][0]);
    glUniformMatrix4fv(Shaders::debug.uniform("model"), 1, GL_FALSE, &w->position[0]);
    glUniform4f(Shaders::debug.uniform("color"), 1.0, 1.0, 1.0, 1.0);
    glUniform4f(Shaders::debug.uniform("color_flash_to"), 1.0, 0.0, 1.0, 1.0);
    glUniform1f(Shaders::debug.uniform("time"), glfwGetTime());
    glUniform1f(Shaders::debug.uniform("shaded"), 0.0);
    glUniform1f(Shaders::debug.uniform("flashing"), flash ? 1.0: 0.0);

    if (graphics::water_grid.complete) {
        gl_state.bind_vao(graphics::water_grid.vao);
        glDrawElements(graphics::water_grid.draw_mode, graphics::water_grid.draw_count[0], graphics::water_grid.draw_type, (GLvoid*)(sizeof(GLubyte)*graphics::water_grid.draw_start[0]));
    }
   
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void drawMeshCube(const glm::vec3 &pos, const glm::quat &rot, const glm::mat3x3 &scl, const Camera &camera){
    bindBackbuffer();
    gl_state.set_flags(GlFlags::CULL | GlFlags::DEPTH_READ);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glEnable(GL_LINE_SMOOTH);
    glLineWidth(1.0);

    gl_state.bind_program(Shaders::debug.program());
    auto mvp = camera.vp * createModelMatrix(pos, rot, scl);
    glUniformMatrix4fv(Shaders::debug.uniform("mvp"), 1, GL_FALSE, &mvp[0][0]);
    glUniformMatrix4fv(Shaders::debug.uniform("model"), 1, GL_FALSE, &pos[0]);
    glUniform4f(Shaders::debug.uniform("color"), 1.0, 1.0, 1.0, 1.0);
    glUniform4f(Shaders::debug.uniform("color_flash_to"), 1.0, 0.0, 1.0, 1.0);
    glUniform1f(Shaders::debug.uniform("time"), glfwGetTime());
    glUniform1f(Shaders::debug.uniform("shaded"), 0.0);
    glUniform1f(Shaders::debug.uniform("flashing"), 0.0);

    drawCube();

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void drawFrustrum(Camera &drawn_camera, const Camera& camera) {
    bindBackbuffer();
    gl_state.set_flags(GlFlags::CULL);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glEnable(GL_LINE_SMOOTH);

    gl_state.bind_program(Shaders::debug.program());
    // Transform into drawn camera's view space, then into world space
    auto projection = glm::perspective(drawn_camera.frustrum.fov, drawn_camera.frustrum.aspect_ratio, drawn_camera.frustrum.near_plane, 2.0f);
    auto model = glm::inverse(projection*drawn_camera.view);
    auto mvp = camera.vp * model;
    glUniformMatrix4fv(Shaders::debug.uniform("mvp"), 1, GL_FALSE, &mvp[0][0]);
    glUniformMatrix4fv(Shaders::debug.uniform("model"), 1, GL_FALSE, &model[0][0]);
    glUniform4f(Shaders::debug.uniform("color"), 1.0, 0.0, 1.0, 0.8);
    glUniform4f(Shaders::debug.uniform("color_flash_to"), 1.0, 0.0, 1.0, 1.0);
    glUniform1f(Shaders::debug.uniform("time"), glfwGetTime());
    glUniform1f(Shaders::debug.uniform("shaded"), 0.0);
    glUniform1f(Shaders::debug.uniform("flashing"), 0.0);

    drawLineCube();

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void drawMeshWireframe(const Mesh &mesh, const glm::mat4& g_model_rot_scl, const glm::mat4& g_model_pos, const Camera &camera, bool flash = false, int submesh_i = -1){
    bindBackbuffer();
    gl_state.set_flags(GlFlags::CULL);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glEnable(GL_LINE_SMOOTH);

    gl_state.bind_program(Shaders::debug.program());
    glUniform4f(Shaders::debug.uniform("color"), 1.0, 1.0, 1.0, 1.0);
    glUniform4f(Shaders::debug.uniform("color_flash_to"), 1.0, submesh_i != -1 ? 1.0 : 0.0, submesh_i != -1 ? 0.0 : 1.0, 1.0);
    glUniform1f(Shaders::debug.uniform("time"), glfwGetTime());
    glUniform1f(Shaders::debug.uniform("shaded"), 0.0);
    glUniform1f(Shaders::debug.uniform("flashing"), flash ? 1.0: 0.0);

    gl_state.bind_vao(mesh.vao);
    
    for (int j = 0; j < mesh.num_submeshes; ++j) {
        if (submesh_i != -1 && submesh_i >= 0 && submesh_i < mesh.num_submeshes) 
            j = submesh_i;
        // Since the mesh transforms encode scale this will mess up global translation so we apply translation after
        auto model = g_model_pos * mesh.transforms[j] * g_model_rot_scl;
        auto mvp = camera.vp * model;
        glUniformMatrix4fv(Shaders::debug.uniform("mvp"), 1, GL_FALSE, &mvp[0][0]);
        glUniformMatrix4fv(Shaders::debug.uniform("model"), 1, GL_FALSE, &model[0][0]);

        glDrawElements(mesh.draw_mode, mesh.draw_count[j], mesh.draw_type, (GLvoid*)(sizeof(*mesh.indices)*mesh.draw_start[j]));

        if (submesh_i != -1)
            break;
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void drawEditor3DRing(const glm::vec3 &position, const glm::vec3 &direction, const Camera &camera, const glm::vec4 &color, const glm::vec3 &scale, bool shaded){
    bindBackbuffer();
    gl_state.set_flags(GlFlags::DEPTH_READ | GlFlags::DEPTH_READ | GlFlags::CULL | GlFlags::BLEND);

    glEnablei(GL_BLEND, graphics::hdr_fbo);
    glBlendFunci(graphics::hdr_fbo, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  

    gl_state.bind_program(Shaders::debug.program());

    const auto mesh_up = glm::vec3(0, 1, 0);
    auto transform = createModelMatrix(position, glm::quat(direction, mesh_up), scale);

    auto mvp = camera.vp * transform;
    glUniformMatrix4fv(Shaders::debug.uniform("mvp"), 1, GL_FALSE, &mvp[0][0]);
    glUniformMatrix4fv(Shaders::debug.uniform("model"), 1, GL_FALSE, &transform[0][0]);
    glUniform3fv(Shaders::debug.uniform("sun_direction"), 1, &loaded_level.environment.sun_direction[0]);
    glUniform4fv(Shaders::debug.uniform("color"), 1, &color[0]);
    glUniform1f(Shaders::debug.uniform("shaded"), shaded ? 1.0 : 0.0);
    glUniform1f(Shaders::debug.uniform("flashing"), 0);
    gl_state.bind_vao(ring_mesh.vao);
    glDrawElements(ring_mesh.draw_mode, ring_mesh.draw_count[0], ring_mesh.draw_type, (GLvoid*)(sizeof(GLubyte)*ring_mesh.draw_start[0]));
}

void drawEditor3DArrow(const glm::vec3 &position, const glm::vec3 &direction, const Camera &camera, const glm::vec4 &color, const glm::vec3 &scale, bool shaded, bool block){
    bindBackbuffer();
    gl_state.set_flags(GlFlags::DEPTH_READ | GlFlags::DEPTH_READ | GlFlags::CULL | GlFlags::BLEND);

    glEnablei(GL_BLEND, graphics::hdr_fbo);
    glBlendFunci(graphics::hdr_fbo, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  

    gl_state.bind_program(Shaders::debug.program());

    const auto mesh_up = glm::vec3(0, 1, 0);
    auto transform = createModelMatrix(position, glm::quat(direction, mesh_up), scale);

    auto mvp = camera.vp * transform;
    glUniformMatrix4fv(Shaders::debug.uniform("mvp"), 1, GL_FALSE, &mvp[0][0]);
    glUniformMatrix4fv(Shaders::debug.uniform("model"), 1, GL_FALSE, &transform[0][0]);
    glUniform3fv(Shaders::debug.uniform("sun_direction"), 1, &loaded_level.environment.sun_direction[0]);
    glUniform4fv(Shaders::debug.uniform("color"), 1, &color[0]);
    glUniform1f(Shaders::debug.uniform("shaded"), shaded ? 1.0 : 0.0);
    glUniform1f(Shaders::debug.uniform("flashing"), 0);
    if(!block){
        gl_state.bind_vao(arrow_mesh.vao);
        glDrawElements(arrow_mesh.draw_mode, arrow_mesh.draw_count[0], arrow_mesh.draw_type, (GLvoid*)(sizeof(GLubyte)*arrow_mesh.draw_start[0]));
    } else {
        gl_state.bind_vao(block_arrow_mesh.vao);
        glDrawElements(block_arrow_mesh.draw_mode, block_arrow_mesh.draw_count[0], block_arrow_mesh.draw_type, (GLvoid*)(sizeof(GLubyte)*block_arrow_mesh.draw_start[0]));

    }
}

void drawColliders(const EntityManager &entity_manager, const Camera &camera) {
    bindBackbuffer();
    glLineWidth(1);

    gl_state.bind_program(Shaders::debug.program());
    glUniform4f(Shaders::debug.uniform("color"), 0.0, 1.0, 1.0, 0.2);
    glUniform1f(Shaders::debug.uniform("time"), glfwGetTime());
    glUniform1f(Shaders::debug.uniform("shaded"), 0.0);
    glUniform1f(Shaders::debug.uniform("flashing"), 0.0);

    for (int i = 0; i < ENTITY_COUNT; ++i) {
        auto c = (ColliderEntity*)entity_manager.entities[i];
        if (c == nullptr || !(entityInherits(c->type, COLLIDER_ENTITY))) continue;
    
        auto model = createModelMatrix(c->collider_position, c->collider_rotation, c->collider_scale);
        auto mvp = camera.vp * model;
        glUniformMatrix4fv(Shaders::debug.uniform("mvp"), 1, GL_FALSE, &mvp[0][0]);
        glUniformMatrix4fv(Shaders::debug.uniform("model"), 1, GL_FALSE, &model[0][0]);
        
        drawLineCube();
    }
}

static void drawInfoTextGui() {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(window_width, window_height));
    ImGui::Begin("Perf Counter", NULL, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration);
    ImGui::TextWrapped("%s\n%.3f ms/frame (%.1f FPS)",
        gl_state.renderer.c_str(), 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
    if (loaded_level.path != "")
        ImGui::TextWrapped("%s\n", loaded_level.path.c_str());
    if (use_level_camera)
        ImGui::TextWrapped("Editing level camera!\n");
    ImGui::PopStyleColor();

    for (int i = 0; i < info_message_queue.size(); i++) {
        auto& m = info_message_queue[i];

        // @hardcoded
        if (i < 10) {
            float alpha = glm::smoothstep(m.duration, m.duration * 0.8f, (float)glfwGetTime() - m.time);

            if(m.urgency != InfoMessage::Urgency::NORMAL)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, alpha));
            else
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, alpha));
            ImGui::TextWrapped(m.contents.c_str());
            ImGui::PopStyleColor();
        }

        if (glfwGetTime() - m.time > m.duration) {
            info_message_queue.erase(info_message_queue.begin() + i);
            i--;
        }
    }

    ImGui::End();
}

void drawEditorGui(EntityManager &entity_manager, AssetManager &asset_manager){
    Camera& camera = *Cameras::get_active_camera();
    if (!use_level_camera && draw_level_camera) {
        loaded_level.camera.update();
        drawFrustrum(loaded_level.camera, camera);
    }
    
    if (draw_colliders) {
        drawColliders(entity_manager, camera);
    }
    if (draw_debug_wireframe) {
        for (const auto& id : selection.ids) {
            auto e = entity_manager.getEntity(id);
            
            if (entityInherits(e->type, WATER_ENTITY)) {
                drawWaterDebug((WaterEntity*)e, camera, true);
            }
            if (e != nullptr && (entityInherits(e->type, MESH_ENTITY))) {
                auto m_e = (MeshEntity*)e;
                if (m_e->mesh != nullptr) {
                    auto g_model_rot_scl = glm::mat4_cast(m_e->rotation) * glm::mat4x4(m_e->scale);
                    auto g_model_pos = glm::translate(glm::mat4x4(1.0), m_e->position);
                    drawMeshWireframe(*m_e->mesh, g_model_rot_scl, g_model_pos, camera, true, selection.submesh_i);
                }

            }
        }
    }
    
    // Start the Dear ImGui frame;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    constexpr float pad = 10;
    const static float sidebar_open_len = 0.2; // s
    static float sidebar_pos_right = 0;
    static float sidebar_open_time;

    static Texture** selected_texture = NULL;
    {
        if(selection.ids.size()) {
            // @todo fix when fe is null
            auto fe = (Entity*)entity_manager.getEntity(selection.ids[0]);

            // Just opened
            if(sidebar_pos_right == 0) {
                sidebar_open_time = glfwGetTime();
            }
            float sidebar_w = std::min(350.0f, window_width/2.0f);
            auto mix_t = glm::smoothstep(0.0f, sidebar_open_len, (float)glfwGetTime() - sidebar_open_time);
            sidebar_pos_right = glm::mix(sidebar_pos_right, sidebar_w, mix_t);

            ImGui::SetNextWindowPos(ImVec2(window_width-sidebar_pos_right,0));
            ImGui::SetNextWindowSize(ImVec2(sidebar_w,window_height));
            ImGui::SetNextWindowSizeConstraints(ImVec2(sidebar_w, window_height), ImVec2(window_width / 2.0, window_height));

            sidebar_w = ImGui::GetWindowWidth();

            ImGui::Begin("###entity", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar);
            if (selection.ids.size() == 1) {
                std::string entity_type = "Unknown Type";
                auto lu = entity_type_to_string.find(fe->type);
                if (lu != entity_type_to_string.end()) {
                    entity_type = lu->second;
                }

                ImGui::TextWrapped("Entity Index: %d Version: %d, %s", (int)selection.ids[0].i, (int)selection.ids[0].v, entity_type.c_str());
            }
            else 
                ImGui::TextWrapped("Multiple Entities Selected");

            const float img_w = glm::min(sidebar_w - pad, 70.0f);
            static const std::vector<std::string> image_file_extensions = { ".jpg", ".png", ".bmp", ".tiff", ".tga" };
            static bool editing_gizmo_position_offset = false;

            auto button_size = ImVec2(ImGui::GetWindowWidth() - 2.0f * pad, 2.0f * pad);
            auto h_button_size = ImVec2(ImGui::GetWindowWidth() / 2.0f - pad, 2.0f * pad);
            if (entityInherits(selection.type, MESH_ENTITY)) {
                auto m_e = (MeshEntity*)fe;

                TransformType edited_transform;
                if (!editing_gizmo_position_offset) {
                    if (selection.ids.size() == 1) {
                        auto pos = m_e->position + m_e->gizmo_position_offset;
                        edited_transform = editTransform(camera, pos, m_e->rotation, m_e->scale);
                        m_e->position = pos - m_e->gizmo_position_offset;

                        selection.avg_position = m_e->position;
                    }
                    else {
                        glm::vec3 position = m_e->position + m_e->gizmo_position_offset;
                        glm::quat rotation = m_e->rotation;
                        glm::mat3 scale = m_e->scale;
                        
                        // @todo support multiple selection rotation and scale
                        edited_transform = editTransform(camera, position, rotation, scale, TransformType::POS);

                        auto offset = position - m_e->position - m_e->gizmo_position_offset;
                        for (const auto& id : selection.ids) {
                            auto e = (MeshEntity*)entity_manager.getEntity(id);
                            if (e == nullptr) continue;
                            e->position += offset;
                        }
                        selection.avg_position += offset;
                    }
                }
                else {
                    auto pos = m_e->position + m_e->gizmo_position_offset;
                    glm::quat _unused_quat;
                    glm::mat3 _unused_mat;
                    edited_transform = editTransform(camera, pos, _unused_quat, _unused_mat, TransformType::POS);
                    m_e->gizmo_position_offset = pos - m_e->position;
                }
                transform_active = edited_transform != TransformType::NONE;

                ImGui::Checkbox("Gizmo Offset", &editing_gizmo_position_offset);

                bool casts_shadow = m_e->casts_shadow;
                if (ImGui::Checkbox("Casts Shadows", &casts_shadow)) {
                    for (const auto& id : selection.ids) {
                        auto e = (MeshEntity*)entity_manager.getEntity(id);
                        e->casts_shadow = casts_shadow;
                    }
                }
                if (entityInherits(selection.type, COLLIDER_ENTITY)) {
                    auto c_e = (ColliderEntity*)m_e;
                    bool selectable = c_e->selectable;
                    if (ImGui::Checkbox("Selectable", &selectable)) {
                        for (const auto& id : selection.ids) {
                            auto e = (ColliderEntity*)entity_manager.getEntity(id);
                            e->selectable = selectable;
                        }
                    }
                }
                
                bool do_lightmap = selection.ids.size() ? m_e->do_lightmap : true;
                for (const auto& id : selection.ids) { 
                    auto e = (MeshEntity*)entity_manager.getEntity(id);
                    if (e && !e->do_lightmap) {
                        do_lightmap = false;
                        break;
                    }
                }
                if (ImGui::Checkbox("Do Lightmap", &do_lightmap)) {
                    for (const auto& id : selection.ids) {
                        auto e = (MeshEntity*)entity_manager.getEntity(id);
                        if (e) {
                            e->do_lightmap = do_lightmap;
                        }
                    }
                }

                if (selection.ids.size() == 1 && m_e->mesh && m_e->mesh->complete) {
                    auto& mesh = m_e->mesh;

                    if (ImGui::Button(mesh->handle.c_str(), button_size)) {
                        im_file_dialog.SetPwd(exepath + "/data/mesh");
                        im_file_dialog_type = "changeMesh";
                        im_file_dialog.SetCurrentTypeFilterIndex(10);
                        im_file_dialog.SetTypeFilters({ ".mesh" });
                        im_file_dialog.Open();
                    }

                    if (mesh->num_submeshes) {
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetTextLineHeight());
                        std::vector<char*> submesh_names;
                        submesh_names.resize(mesh->num_submeshes + 1);
                        static const char* none_str = "*none*";
                        submesh_names[0] = (char*)none_str;
                        for (int i = 0; i < mesh->num_submeshes; i++) 
                            submesh_names[i+1] = (char*)mesh->submesh_names[i].c_str();
                    
                        ImGui::Text("Choose submesh: ");
                        ImGui::SetNextItemWidth(ImGui::GetColumnWidth());
                        int si = selection.submesh_i;
                        if (ImGui::ListBox("##submesh-listbox", &si, &submesh_names[0], (int)mesh->num_submeshes + 1, glm::min((int)mesh->num_submeshes + 1, 4))) {
                            selection.setSubmesh(si-1);
                        }
                    }

                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetTextLineHeight());
                    if (selection.submesh_i != -1 && ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
                        ImGui::Indent(20.0);

                        const auto create_tex_ui = [&img_w, &sidebar_w, &asset_manager](Texture*& tex, TextureSlot slot) {
                            if (!tex)
                                return false;
                            std::string id = tex->handle + std::to_string((uint64_t)slot);

                            static int channels = (int)getChannelsForFormat(tex->format);
                            ImGui::Text("Channels: ");
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(ImGui::GetColumnWidth() - ImGui::CalcTextSize("Channels: ").x);
                            if (ImGui::InputInt(("###channels-" + id).c_str(), &channels, 1, 1)) {
                                if (channels >= 1 && channels <= 4) {
                                    if (channels < (int)getChannelsForFormat(tex->format)) {
                                        tex->format = getFormatForChannels((ImageChannels)channels);
                                    }
                                    else {
                                        tex->format = getFormatForChannels((ImageChannels)channels);
                                        if (tex->is_color) {
                                            tex = asset_manager.getColorTexture(tex->color, tex->format);
                                        }
                                        else {
                                            asset_manager.loadTexture(tex, tex->handle, tex->format);
                                        }
                                    }
                                }
                                else {
                                    channels = (int)getChannelsForFormat(tex->format);
                                }
                            }

                            if (tex->is_color) {
                                ImGui::Text("Color: ");
                                ImGui::SameLine();
                                ImGui::Checkbox(("###is_color-" + id).c_str(), &tex->is_color);

                                ImGui::SetNextItemWidth(ImGui::GetColumnWidth());
                                if (ImGui::ColorEdit4(("###color-" + id).c_str(), &tex->color.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_DisplayRGB)) {
                                    tex = asset_manager.getColorTexture(tex->color, tex->format);
                                }
                            }
                            else {
                                ImGui::Text("Color: ");
                                ImGui::SameLine();
                                if (ImGui::Checkbox(("###is_color-" + id).c_str(), &tex->is_color)) {
                                    tex = asset_manager.getColorTexture(tex->color, tex->format);
                                }

                                void* tex_id = (void*)(intptr_t)tex->id;
                                if (ImGui::ImageButton(tex_id, ImVec2(ImGui::GetColumnWidth(), ImGui::GetColumnWidth()))) {
                                    im_file_dialog.SetPwd(exepath + "/data/textures");
                                    selected_texture = &tex;
                                    im_file_dialog_type = "mat.tex";
                                    im_file_dialog.SetCurrentTypeFilterIndex(2);
                                    im_file_dialog.SetTypeFilters(image_file_extensions);
                                    im_file_dialog.Open();
                                }
                            }
                        };

                        Material* mat;
                        auto& lu = m_e->overidden_materials.find(selection.submesh_i);
                        if (lu == m_e->overidden_materials.end()) {
                            mat = &mesh->materials[mesh->material_indices[selection.submesh_i]];

                            if (ImGui::Button("Make unique", ImVec2(ImGui::GetColumnWidth(), button_size.y))) {
                                m_e->overidden_materials[selection.submesh_i] = *mat;
                                mat = &m_e->overidden_materials[selection.submesh_i];
                            }
                            if (ImGui::Button("Write to Mesh (Warning!)", ImVec2(ImGui::GetColumnWidth(), button_size.y))) {
                                asset_manager.writeMeshFile(mesh, mesh->handle);
                            }
                            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                                ImGui::SetTooltip("This overwrites the mesh for all levels with any changes");
                            }
                        }
                        else {
                            mat = &lu->second;

                            if (ImGui::Button("Make un-unique (Warning!)", ImVec2(ImGui::GetColumnWidth(), button_size.y))) {
                                m_e->overidden_materials.erase(selection.submesh_i);
                                mat = &mesh->materials[mesh->material_indices[selection.submesh_i]];
                            }
                            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                                ImGui::SetTooltip("This will lose any changes to the material you made");
                            }
                        }

                        ImGui::SetNextItemWidth(ImGui::GetColumnWidth());
                        auto mat_type = mat->type & (MaterialType::PBR | MaterialType::BLINN_PHONG | MaterialType::NONE);
                        std::string mat_type_name = getMaterialTypeName(mat_type);
                        if (!!mat_type && ImGui::BeginCombo("##mat-type-combo", mat_type_name.c_str())) {
                            const MaterialType mat_list[2] = { MaterialType::PBR, MaterialType::BLINN_PHONG };

                            for (int i = 0; i < 2; i++) {
                                auto type = mat_list[i];
                                bool is_selected = !!(mat_type & type);

                                if (ImGui::Selectable(getMaterialTypeName(type).c_str(), is_selected) && !is_selected) {
                                    mat->type &= ~mat_type; // Get rid of previous material
                                    mat->type |= type; // Add new one

                                    // Get rid of old uniforms, worry about textures later (since this will serialize unnecessary texture)
                                    // @todo better way, this could be cleaner if materials were assets which we switch between rather than
                                    // editing in place
                                    switch (mat_type)
                                    {
                                    case MaterialType::PBR:
                                        mat->uniforms.erase("albedo_mult");
                                        mat->uniforms.erase("roughness_mult");
                                        break;
                                    case MaterialType::BLINN_PHONG:
                                        mat->uniforms.erase("diffuse_mult");
                                        mat->uniforms.erase("specular_mult");
                                        mat->uniforms.erase("shininess_mult");
                                        break;
                                    }
                                    switch (type)
                                    {
                                    case MaterialType::PBR:
                                        mat->uniforms.emplace("albedo_mult", default_material->uniforms["albedo_mult"]);
                                        mat->uniforms.emplace("roughness_mult", default_material->uniforms["roughness_mult"]);
                                        break;
                                    case MaterialType::BLINN_PHONG:
                                        mat->uniforms.emplace("diffuse_mult", default_material->uniforms["diffuse_mult"]);
                                        mat->uniforms.emplace("specular_mult", default_material->uniforms["specular_mult"]);
                                        mat->uniforms.emplace("shininess_mult", default_material->uniforms["shininess_mult"]);
                                        break;
                                    }
                                }
                                if (is_selected)
                                    ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                        
                        const auto make_type_ui = [](Material& mat, MaterialType type) {
                            auto type_name = getMaterialTypeName(type);

                            bool is_type = !!(mat.type & type);
                            if (ImGui::Checkbox(type_name.c_str(), &is_type)) {
                                mat.type = is_type ? (mat.type | type) : (mat.type & (~type));

                                static const auto add_mat_type = [&is_type, &type](Material& mat, TextureSlot slot, std::string uniform) {
                                    if (is_type) {
                                        if (uniform != "")
                                            mat.uniforms.emplace(uniform, default_material->uniforms[uniform]);
                                        mat.textures.emplace(slot, default_material->textures[slot]);
                                    }
                                    else {
                                        if (uniform != "")
                                            mat.uniforms.erase(uniform);
                                        mat.textures.erase(slot);
                                    }
                                };

                                switch (type)
                                {
                                case MaterialType::EMISSIVE:
                                    add_mat_type(mat, TextureSlot::EMISSIVE, "emissive_mult");
                                    break;
                                case MaterialType::AO:
                                    add_mat_type(mat, TextureSlot::AO, "ambient_mult");
                                    break;
                                case MaterialType::LIGHTMAPPED:
                                    add_mat_type(mat, TextureSlot::GI, "ambient_mult");
                                    break;
                                case MaterialType::METALLIC:
                                    add_mat_type(mat, TextureSlot::METAL, "metal_mult");
                                    break;

                                default:
                                    break;
                                }
                            }
                        };
                        ImGui::Columns(2, "Material Types");
                        make_type_ui(*mat, MaterialType::EMISSIVE);
                        make_type_ui(*mat, MaterialType::AO);
                        make_type_ui(*mat, MaterialType::LIGHTMAPPED);
                        make_type_ui(*mat, MaterialType::METALLIC);

                        ImGui::NextColumn();
                        make_type_ui(*mat, MaterialType::VEGETATION);
                        make_type_ui(*mat, MaterialType::ALPHA_CLIP);
                        ImGui::Columns();

                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetTextLineHeight());
                        if (ImGui::CollapsingHeader("Textures:")) {
                            ImGui::Indent(10.0);
                            for(auto& p : mat->textures) {
                                auto& slot = p.first;
                                auto& tex = p.second;
                            
                                if (ImGui::CollapsingHeader(getTextureSlotName(*mat, slot).c_str())) {
                                    if (!create_tex_ui(tex, slot)) {
                                        if (ImGui::Button("Make texture", ImVec2(ImGui::GetColumnWidth(), button_size.y))) {
                                            tex = asset_manager.getColorTexture(glm::vec4(1.0), GL_RGBA);
                                        }
                                    }
                                }
                            }
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetTextLineHeight());
                            ImGui::Unindent(10.0);
                        }

                        
                        if (ImGui::CollapsingHeader("Uniforms:")) {
                            ImGui::Indent(10.0);
                            for (auto& p : mat->uniforms) {
                                auto& name = p.first;
                                auto& uniform = p.second;

                                ImGui::SetNextItemWidth(ImGui::GetColumnWidth() - ImGui::CalcTextSize(name.c_str()).x);
                                switch (uniform.type) {
                                case Uniform::Type::FLOAT:
                                    ImGui::SliderFloat(name.c_str(), (float*)uniform.data, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_ClampOnInput);
                                    break;
                                case Uniform::Type::VEC3:
                                    ImGui::ColorEdit3(name.c_str(), (float*)uniform.data, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_DisplayRGB);
                                    break;
                                case Uniform::Type::VEC4:
                                    ImGui::ColorEdit4(name.c_str(), (float*)uniform.data, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_DisplayRGB);
                                    break;

                                default:
                                    ImGui::Text(name.c_str());
                                    break;
                                }
                            }
                            ImGui::Unindent(10.0);
                        }
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetTextLineHeight());
                        ImGui::Unindent(20.0);
                    }
                }
            }
            if (entityInherits(selection.type, WATER_ENTITY) && selection.ids.size() == 1) {
                auto w_e = (WaterEntity*)fe;
                
                glm::quat _r = glm::quat();
                transform_active = editTransform(camera, w_e->position, _r, w_e->scale, TransformType::POS_SCL) != TransformType::NONE;
                ImGui::TextWrapped("Shallow Color:");
                ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - pad);
                ImGui::ColorEdit4("##shallow_color", (float*)(&w_e->shallow_color));
                ImGui::TextWrapped("Deep Color:");
                ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - pad);
                ImGui::ColorEdit4("##deep_color", (float*)(&w_e->deep_color));
                ImGui::TextWrapped("Foam Color:");
                ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - pad);
                ImGui::ColorEdit4("##foam_color", (float*)(&w_e->foam_color));

                if (ImGui::CollapsingHeader("Noise Textures")) {
                    ImGui::Text("Gradient");
                    ImGui::SameLine();
                    auto cursor = ImGui::GetCursorPos();
                    cursor.x += img_w;
                    ImGui::SetCursorPos(cursor);
                    ImGui::Text("Value");
                    void* tex_simplex_gradient = (void*)(intptr_t)graphics::simplex_gradient->id;
                    if (ImGui::ImageButton(tex_simplex_gradient, ImVec2(img_w, img_w))) {
                        im_file_dialog.SetPwd(exepath + "/data/textures");
                        im_file_dialog_type = "simplexGradient";
                        im_file_dialog.SetCurrentTypeFilterIndex(2);
                        im_file_dialog.SetTypeFilters(image_file_extensions);
                        im_file_dialog.Open();
                    }
                    ImGui::SameLine();
                    void* tex_simplex_value = (void*)(intptr_t)graphics::simplex_value->id;
                    if (ImGui::ImageButton(tex_simplex_value, ImVec2(img_w, img_w))) {
                        im_file_dialog.SetPwd(exepath + "/data/textures");
                        im_file_dialog_type = "simplexValue";
                        im_file_dialog.SetCurrentTypeFilterIndex(2);
                        im_file_dialog.SetTypeFilters(image_file_extensions);
                        im_file_dialog.Open();
                    }
                }
                void* tex_water_collider = (void*)(intptr_t)graphics::water_collider_buffers[graphics::water_collider_final_fbo];
                ImGui::Image(tex_water_collider, ImVec2(sidebar_w, sidebar_w));

                if (ImGui::Button("Update", button_size)) {
                    RenderQueue q;
                    createRenderQueue(q, entity_manager);
                    bindDrawWaterColliderMap(q, w_e);
                    distanceTransformWaterFbo(w_e);
                }
            }
            if (entityInherits(selection.type, ANIMATED_MESH_ENTITY) && selection.ids.size() == 1) {
                auto a_e = (AnimatedMeshEntity*)fe;

                if (ImGui::CollapsingHeader("Animations")) {
                    ImGui::Indent(10.0);
                    if (a_e->animesh != nullptr) {
                        std::string animation_name = a_e->default_event.animation == nullptr ? "None" : a_e->default_event.animation->name;
                        ImGui::SetNextItemWidth(ImGui::GetColumnWidth());
                        if (ImGui::BeginCombo("##asset-combo", animation_name.c_str())) {
                            for (auto& p : a_e->animesh->name_animation_map) {
                                bool is_selected = (&p.second == a_e->default_event.animation);
                                if (ImGui::Selectable(p.first.c_str(), is_selected))
                                    a_e->default_event.animation = &p.second;
                                if (is_selected)
                                    ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }
                    }

                    bool draw_animated = false; // @debug Used by editor to toggle drawing animation, ignored when playing
                    float current_time = 0.0f;
                    float time_scale = 1.0f;
                    bool loop = false;
                    bool playing = false;
                    if (a_e->default_event.animation != nullptr) {
                        ImGui::Checkbox("Draw Animated: ", &a_e->draw_animated);
                        if (ImGui::SliderFloat("Current Time: ", &a_e->default_event.current_time, 0.0f, a_e->default_event.animation->duration, "%.3f")) {
                            a_e->init();
                        }
                        ImGui::SliderFloat("Time Scale: ", &a_e->default_event.time_scale, -10.0f, 10.0f, "%.3f");
                        ImGui::Checkbox("Loop: ", &a_e->default_event.loop);
                        ImGui::Checkbox("Playing: ", &a_e->default_event.playing);
                    }

                    ImGui::Unindent(10.0);
                }
            }

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetTextLineHeight());
            bool half = false;
            if(!(entityInherits(selection.type, WATER_ENTITY))){
                if (ImGui::Button("Duplicate", h_button_size)) {
                    if (camera.state == Camera::Type::TRACKBALL && entityInherits(selection.type, MESH_ENTITY)) {
                        auto m_e = (MeshEntity*)fe;
                        selection.avg_position += translation_snap.x;
                    }

                    for (auto& id : selection.ids) {
                        auto e = entity_manager.duplicateEntity(id);
                        if (entityInherits(selection.type, MESH_ENTITY)) {
                            auto m_e = reinterpret_cast<MeshEntity*>(e);
                            m_e->position.x += translation_snap.x;   
                        }   
                        id = e->id;
                    }
                }
                ImGui::SameLine();
                half = true;
            }
            if(ImGui::Button("Delete", half ? h_button_size : button_size)){
                for (auto& id : selection.ids) {
                    entity_manager.deleteEntity(id);
                    if (id == entity_manager.water) {
                        entity_manager.water = NULLID;
                    }
                }
                selection.clear();
            }
            
            ImGui::End();
        } else {
            sidebar_pos_right = 0;
        }
    }

    drawInfoTextGui();
    ImTerminal(entity_manager, asset_manager, do_terminal);

    // Handle imfile dialog browser
    {
        im_file_dialog.Display();
        if(im_file_dialog.HasSelected())
        {
            auto p = im_file_dialog.GetSelected().string().erase(0, exepath.length() + 1);
            std::replace(p.begin(), p.end(), '\\', '/');
            std::cout << "Selected filename at path " << p << ".\n";
            if (im_file_dialog_type == "loadLevel") {
                std::string dummy;
                loadLevelCommand(std::vector<std::string>{ "load_level", p }, dummy, entity_manager, asset_manager);
            } else if (im_file_dialog_type == "changeMesh") {
                auto mesh = asset_manager.getMesh(p);
                if (mesh == nullptr) {
                    mesh = asset_manager.createMesh(p);
                    asset_manager.loadMeshFile(mesh, p);
                }
                for (const auto& id : selection.ids) {
                    auto e = (MeshEntity*)entity_manager.getEntity(id);
                    if (e == nullptr) continue;

                    e->mesh = mesh;
                }
            } else if(im_file_dialog_type == "loadModelAssimp"){
                auto mesh = asset_manager.createMesh(p);
                asset_manager.loadMeshAssimp(mesh, p);
            } else if (im_file_dialog_type == "simplexValue") {
                global_assets.loadTexture(graphics::simplex_value, p, GL_RED);
            } else if (im_file_dialog_type == "simplexGradient") {
                global_assets.loadTexture(graphics::simplex_gradient, p, GL_RGB);
            } else if(im_file_dialog_type == "mat.tex") {
                if (selected_texture) {
                    // Assets might already been loaded so just use it
                    (*selected_texture) = asset_manager.getTexture(p);
                    if((*selected_texture) == nullptr) {
                        (*selected_texture) = asset_manager.createTexture(p);
                        asset_manager.loadTexture((*selected_texture), p);
                    }
                }
            } else {
                std::cerr << "Unhandled imgui file dialog type at path " + p + ".\n";
            }

            im_file_dialog.ClearSelected();
        }
    }

    // Rendering ImGUI
    ImGui::Render();
    auto &io = ImGui::GetIO();
    gl_state.bind_viewport(io.DisplaySize.x, io.DisplaySize.y);
    // Alpha coverage causes a stippling patern in the UI which looks bad since imgui seems to disable alpha entirely
    gl_state.remove_flags(GlFlags::ALPHA_COVERAGE);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void drawGameGui(EntityManager& entity_manager, AssetManager& asset_manager) {
    // Start the Dear ImGui frame;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    drawInfoTextGui();

    ImTerminal(entity_manager, asset_manager, do_terminal);

    // Rendering ImGUI
    ImGui::Render();
    auto& io = ImGui::GetIO();
    gl_state.bind_viewport(io.DisplaySize.x, io.DisplaySize.y);
    // This causes a stippling patern which looks bad since imgui seems to disable alpha entirely
    gl_state.remove_flags(GlFlags::ALPHA_COVERAGE);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
