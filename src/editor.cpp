#include "globals.hpp"

#include <stack>
#include <limits>
#include <algorithm>
#include <filesystem>

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

#include "editor.hpp"
#include "utilities.hpp"
#include "graphics.hpp"
#include "shader.hpp"
#include "texture.hpp"
#include "assets.hpp"
#include "controls.hpp"
#include "entities.hpp"
#include "level.hpp"
#include "game_behaviour.hpp"

namespace editor {
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

    ReferenceSelection selection;
    CopySelection copy_selection;
}
using namespace editor;

void ReferenceSelection::addEntity(Entity* e) {
    for (const auto &id : ids) {
        if (id == e->id) return;
    }

    ids.push_back(e->id);
    if (type == ENTITY) type = e->type;
    else                type = (EntityType)(type & e->type);

    if (entityInherits(e->type, MESH_ENTITY)) {
        avg_position = (float)avg_position_count * avg_position + ((MeshEntity*)e)->position;
        avg_position_count++;
        avg_position /= (float)avg_position_count;
    }
}
// If entity is not already in selection add it, else remove it
void ReferenceSelection::toggleEntity(const EntityManager &entity_manager, Entity* e) {
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
            avg_position = (float)avg_position_count * avg_position - ((MeshEntity*)e)->position;
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
            avg_position = (float)avg_position_count * avg_position + ((MeshEntity*)e)->position;
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
            m_e->position += editor::translation_snap;
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
    (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    
    // create a file browser instance
    im_file_dialog_type = "";

    auto &style = ImGui::GetStyle();

    // Setup Dear ImGui style
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.FramePadding = ImVec2(3.0, 4.0);
    style.ItemSpacing  = ImVec2(2.0, 8.0);
    style.ItemInnerSpacing = ImVec2(1.0, 1.0);
    style.CellPadding = ImVec2(1.0, 6.0);
    style.Alpha = 0.9;
    
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.36f, 0.42f, 0.47f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImGuiCol_Border] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.12f, 0.20f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.09f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.12f, 0.14f, 0.65f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.39f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.18f, 0.22f, 0.25f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.09f, 0.21f, 0.31f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.25f, 0.29f, 0.55f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
    colors[ImGuiCol_Tab] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version.c_str());

    asset_manager.loadMeshFile(&arrow_mesh, "data/mesh/arrow.mesh");
    asset_manager.loadMeshFile(&block_arrow_mesh, "data/mesh/block_arrow.mesh");
    asset_manager.loadMeshFile(&ring_mesh, "data/mesh/ring.mesh");
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
        if (loadLevel(level_entity_manager, asset_manager, filename, level_camera)) {
            if(playing) {
                resetGameEntities();
            }
            level_path = filename;
            output += "Loaded level at path " + filename;
            selection.clear();
            editor_camera = level_camera;
            return true;
        }

        output += "Failed to loaded level at path " + filename;
        return false;
    }

    // @note
    editor_camera = level_camera;
    editor_camera.state = Camera::TYPE::STATIC;
    
    output += "Please provide a level name, see list_levels";
    return false;
}

static bool clearLevelCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager) {
    level_entity_manager.clear();
    game_entity_manager.clear();
    editor::selection.clear();
    output += "Cleared level";

    return true;
}

static bool saveLevelCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager) {
    std::string filename;
    if (input_tokens.size() == 1) {
        if (level_path == "") {
            output += "Current level path not set";
            return false;
        }
        filename = level_path;
    }
    else if (input_tokens.size() >= 2) {
        filename = "data/levels/" + input_tokens[1] + ".level";
    }
    saveLevel(level_entity_manager, filename, level_camera);
    level_path = filename;

    output += "Saved current level at path " + filename;
    return true;
}

static bool newLevelCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager) {
    std::string filename;
    if (input_tokens.size() == 1) {
        output += "Please provide a name for the new level";
        return false;
    }
    else if (input_tokens.size() >= 2) {
        level_entity_manager.clear();
        game_entity_manager.clear();
        editor::selection.clear();
        filename = "data/levels/" + input_tokens[1] + ".level";
    }
    level_path = filename;

    saveLevel(level_entity_manager, filename, level_camera);
    output += "Created new level at path " + filename;
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

static bool helpCommand(std::vector<std::string>& input_tokens, std::string& output, EntityManager& entity_manager, AssetManager& asset_manager);

const std::map
<
    std::string,
    std::function<bool(std::vector<std::string> &, std::string &output, EntityManager&, AssetManager&)>
> command_to_func = {
    {"echo", echoCommand},
    {"help", helpCommand},
    {"list_levels", listLevelsCommand},
    {"load_level", loadLevelCommand},
    {"clear_level", clearLevelCommand},
    {"new_level", newLevelCommand},
    {"list_mesh", listMeshCommand},
    {"add_mesh", addMeshCommand},
    {"add_collider", addColliderCommand},
    {"load_model", loadModelCommand},
    {"list_models", listModelsCommand},
    {"save_level", saveLevelCommand},
    {"convert_models_to_mesh", convertModelsToMeshCommand},
    {"add_water", addWaterCommand},
    {"toggle_bloom", toggleBloomCommand},
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
        auto flags = ImGuiInputTextFlags_NoHorizontalScroll | ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_CallbackCharFilter | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_EnterReturnsTrue;
        bool enter_pressed = ImGui::InputTextWithHint("###input_line", "Enter Command", &input_line, flags, Callbacks::callback);
        if(enter_pressed) {
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
        controls::scroll_offset = glm::vec2(0);
    }

    prev_is_active = is_active;
}

TransformType editTransform(Camera &camera, glm::vec3 &pos, glm::quat &rot, glm::mat3 &scl, TransformType type=TransformType::ALL){
    static bool key_t = false, key_r = false, key_s = false, key_n = false;
    static bool use_trans_snap = true, use_rot_snap = true, use_scl_snap = false;
    static glm::vec3 scale_snap = glm::vec3(1.0);
    static float rotation_snap = 90;
    TransformType change_type = TransformType::NONE;

    bool do_p = (bool)((unsigned int)type & (unsigned int)TransformType::POS);
    bool do_r = (bool)((unsigned int)type & (unsigned int)TransformType::ROT);
    bool do_s = (bool)((unsigned int)type & (unsigned int)TransformType::SCL);

    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureKeyboard && camera.state != Camera::TYPE::SHOOTER) {
        if (glfwGetKey(window, GLFW_KEY_T) && !key_t && do_p)
            gizmo_mode = gizmo_mode == GizmoMode::TRANSLATE ? GizmoMode::NONE : GizmoMode::TRANSLATE;
        if (glfwGetKey(window, GLFW_KEY_R) && !key_r && do_r)
            gizmo_mode = gizmo_mode == GizmoMode::ROTATE ? GizmoMode::NONE : GizmoMode::ROTATE;
        if (glfwGetKey(window, GLFW_KEY_S) && !key_s && do_s)
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
        if (!io.WantCaptureKeyboard && glfwGetKey(window, GLFW_KEY_N) && !key_n)
            use_trans_snap = !use_trans_snap;
        ImGui::Checkbox("", &use_trans_snap);
        ImGui::SameLine();
        ImGui::InputFloat3("Snap", &translation_snap[0]);
        if(do_p && editorTranslationGizmo(pos, rot, scl, camera, translation_snap, use_trans_snap))
            change_type = (TransformType)((uint64_t)change_type | (uint64_t)TransformType::POS);
        break;
    case GizmoMode::ROTATE:
        if (!io.WantCaptureKeyboard && glfwGetKey(window, GLFW_KEY_N) && !key_n)
            use_rot_snap = !use_rot_snap;
        ImGui::Checkbox("", &use_rot_snap);
        ImGui::SameLine();
        ImGui::InputFloat("Snap", &rotation_snap);
        if(do_r && editorRotationGizmo(pos, rot, scl, camera, (rotation_snap / 180.f) * PI, use_rot_snap))
            change_type = (TransformType)((uint64_t)change_type | (uint64_t)TransformType::ROT);
        break;
    case GizmoMode::SCALE:
        if (!io.WantCaptureKeyboard && glfwGetKey(window, GLFW_KEY_N) && !key_n)
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

    key_t = glfwGetKey(window, GLFW_KEY_T);
    key_r = glfwGetKey(window, GLFW_KEY_R);
    key_s = glfwGetKey(window, GLFW_KEY_S);
    key_n = glfwGetKey(window, GLFW_KEY_N);
    return change_type;
}

bool editorRotationGizmo(glm::vec3 &pos, glm::quat &rot, glm::mat3 &scl, const Camera &camera, float snap=1.0, bool do_snap=false){
    static int selected_ring = -1;
    static glm::vec3 rot_dir_initial;
    static glm::vec3 rot_dir_prev;
    
    glm::vec3 out_origin;
    glm::vec3 out_direction;
    screenPosToWorldRay(controls::mouse_position, camera.view, camera.projection, out_origin, out_direction);

    glm::vec3 axis[3] = {
        rot*glm::vec3(1,0,0), 
        rot*glm::vec3(0,1,0), 
        rot*glm::vec3(0,0,1)
    };
  
    const float distance = glm::length(pos - camera.position);
    if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE) selected_ring = -1;
    else if(controls::left_mouse_click_press){
        float depth = std::numeric_limits<float>::max();
        for (int i = 0; i < 3; ++i) {
            const glm::fvec3 direction = axis[i];
            //const glm::fvec3 direction(i==0, i==1, i==2);
            const glm::fvec3 scale(0.1-i*0.01, 0.1, 0.1-i*0.01);

            // @copied from drawEditor3DArrow
            // Why?
            auto dir = direction;
            dir.y = -dir.y;
            static const auto ring_direction = glm::vec3(0,-1,0);

            glm::mat4x4 rot;
            if(dir == -ring_direction) rot = glm::rotate(glm::mat4x4(1.0), (float)PI, glm::vec3(1,0,0));
            else                        rot = glm::mat4_cast(glm::quat(dir, ring_direction));
            auto trans = glm::translate(glm::mat4x4(1.0), pos);
            auto scl   = glm::scale(glm::mat4x4(1.0), distance*scale);
            // Scale up so easier to select
            glm::mat4x4 transform =  trans * rot * scl;

            glm::vec3 collision_point(0), normal(0);
            if(rayIntersectsMesh(&ring_mesh, transform, camera, out_origin, out_direction, collision_point, normal)){
                float test_depth = glm::length(collision_point - camera.position);
                if(test_depth < depth){
                    closestDistanceBetweenLineCircle(out_origin, out_direction, pos, direction, scale.x, rot_dir_initial);
                    if(rot_dir_initial == glm::vec3(0.0)) rot_dir_initial = glm::vec3(0,0,0);
                    else                                  rot_dir_initial = glm::normalize(rot_dir_initial - pos);

                    rot_dir_prev = rot_dir_initial;

                    depth = test_depth;
                    selected_ring = i;
                }
            }
        }
    }
    const bool active = selected_ring != -1;

    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);
    
    if(active){
        const glm::fvec3 direction = axis[selected_ring];
        const float radius((0.1-selected_ring*0.01)*distance);

        glm::vec3 rot_dir;
        closestDistanceBetweenLineCircle(out_origin, out_direction, pos, direction, radius, rot_dir);
        if(rot_dir == glm::vec3(0.0)) rot_dir = rot_dir_prev;
        else                          rot_dir = glm::normalize(rot_dir - pos);


        // If ring axis is aligned with rotation axis
        //glm::quat rot = glm::quat(rot_dir_prev - position, rot_dir - position);
        //transform *= glm::mat4_cast(rot);

        // https://nelari.us/post/gizmos/
        glm::vec3 &pp = rot_dir_prev;
        glm::vec3 &pc = rot_dir;
        float angle = glm::abs(glm::acos(glm::clamp(glm::dot(pc, pp), -1.0f, 1.0f)));
        angle *= glm::sign(glm::dot(glm::cross(pp, pc), direction));
        drawEditor3DArrow(pos, pc, camera, glm::vec4(1.0), glm::vec3(distance*0.02, 0.3*radius, distance*0.02), false);
        //drawEditor3DArrow(pos, direction, camera, glm::vec4(1.0), glm::vec3(0.1, 0.3*radius, 0.1));
        
        const glm::fvec3 rotation_axis(selected_ring==0, selected_ring==1, selected_ring==2);

        if(do_snap) {
            drawEditor3DArrow(pos, pp, camera, glm::vec4(0.5), glm::vec3(distance*0.02, 0.3*radius, distance*0.02), false);
            if(angle >= snap || angle <= -snap) {
                angle = glm::floor(angle / snap) * snap;
                rot = glm::rotate(rot, angle, rotation_axis);
                rot_dir_prev = rot_dir;
            }
        } else {
            rot = glm::rotate(rot, angle, rotation_axis);
            rot_dir_prev = rot_dir;
        }
    }

    for (int i = 0; i < 3; ++i) {
        //const glm::fvec3 direction(i==0, i==1, i==2);
        const glm::fvec3 direction = axis[i];
        const glm::fvec3 scale(0.1-i*0.01, 0.1, 0.1-i*0.01);
        const glm::fvec3 color(i==0, i==1, i==2);
        if(selected_ring == i)
            drawEditor3DRing(pos, direction, camera, glm::vec4(color,1.0), distance*scale, false);
        else
            drawEditor3DRing(pos, direction, camera, glm::vec4(color,0.5), distance*scale, false);
    }

    return active;
}

bool editorTranslationGizmo(glm::vec3 &pos, glm::quat &rot, glm::mat3 &scl, Camera &camera, const glm::vec3 &snap, bool do_snap){
    static int selected_arrow = -1;
    static glm::vec3 selection_offset;
    
    glm::vec3 out_origin;
    glm::vec3 out_direction;
    screenPosToWorldRay(controls::mouse_position, camera.view, camera.projection, out_origin, out_direction);
    
    glm::vec3 axis[3] = {
        rot*glm::vec3(1,0,0), 
        rot*glm::vec3(0,-1,0), 
        rot*glm::vec3(0,0,1)
    };

    const float distance = glm::length(camera.position - pos);
    if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE){
        if(camera.state == Camera::TYPE::TRACKBALL){
            camera.target = pos;
            updateCameraView(camera);
        }
        selected_arrow = -1;
    }
    else if(controls::left_mouse_click_press){
        float depth = std::numeric_limits<float>::max();
        for (int i = 0; i < 3; ++i) {
            auto direction = axis[i];
               
            // @copied from drawEditor3DArrow
            // Why?
            auto dir = direction;
            dir.y = -dir.y;
            static const auto arrow_direction = glm::vec3(0,-1,0);

            glm::mat4x4 arrow_rot;
            if(dir == -arrow_direction) arrow_rot = glm::rotate(glm::mat4x4(1.0), (float)PI, glm::vec3(1,0,0));
            else                        arrow_rot = glm::mat4_cast(glm::quat(dir, arrow_direction));
            auto trans = glm::translate(glm::mat4x4(1.0), pos);
            // Scale up so easier to select
            glm::mat4x4 transform =  trans * arrow_rot * glm::scale(glm::mat4x4(1.0), glm::vec3(0.03*distance));

            glm::vec3 collision_point(0), normal(0);
            if(rayIntersectsMesh(&arrow_mesh, transform, camera, out_origin, out_direction, collision_point, normal)){
                float test_depth = glm::length(collision_point - camera.position);
                if(test_depth < depth){
                    depth = test_depth;
                    selection_offset = collision_point - pos;
                    selected_arrow = i;
                }
            }
        }
    }
    const bool active = selected_arrow != -1;

    if(active){
        auto direction = axis[selected_arrow];
        
        float camera_t, direction_t;
        closestDistanceBetweenLines(out_origin, out_direction, pos + selection_offset, direction, camera_t, direction_t);
        auto trans = -direction_t*direction;
        if(do_snap){
            trans.x = glm::round(trans.x / snap.x) * snap.x;
            trans.y = glm::round(trans.y / snap.y) * snap.y;
            trans.z = glm::round(trans.z / snap.z) * snap.z;
        }
        pos += trans;
    }

    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);
    
    for (int i = 0; i < 3; ++i) {
        glm::fvec3 color(i==0, i==1, i==2);
        auto direction = axis[i];

        if(selected_arrow == i)
            drawEditor3DArrow(pos, direction, camera, glm::vec4(color,1.0), glm::vec3(distance*0.03), false);
        else
            drawEditor3DArrow(pos, direction, camera, glm::vec4(color,0.5), glm::vec3(distance*0.03), false);

    }

    return active;
}

bool editorScalingGizmo(glm::vec3 &pos, glm::quat &rot, glm::mat3 &scl, Camera &camera, const glm::vec3 &snap, bool do_snap){
    static int selected_arrow = -1;
    static glm::vec3 selection_offset;
    
    glm::vec3 out_origin;
    glm::vec3 out_direction;
    screenPosToWorldRay(controls::mouse_position, camera.view, camera.projection, out_origin, out_direction);
    
    glm::vec3 axis[3] = {
        rot*glm::vec3(1,0,0), 
        rot*glm::vec3(0,-1,0), 
        rot*glm::vec3(0,0,1)
    };

    const float distance = glm::length(camera.position - pos);
    if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE)
        selected_arrow = -1;
    
    else if(controls::left_mouse_click_press){
        float depth = std::numeric_limits<float>::max();
        for (int i = 0; i < 3; ++i) {
            auto direction = axis[i];
               
            // @copied from drawEditor3DArrow
            // Why?
            auto dir = direction;
            dir.y = -dir.y;
            static const auto block_arrow_direction = glm::vec3(0,-1,0);

            glm::mat4x4 block_arrow_rot;
            if(dir == -block_arrow_direction) block_arrow_rot = glm::rotate(glm::mat4x4(1.0), (float)PI, glm::vec3(1,0,0));
            else                        block_arrow_rot = glm::mat4_cast(glm::quat(dir, block_arrow_direction));
            auto trans = glm::translate(glm::mat4x4(1.0), pos);
            // Scale up so easier to select
            glm::mat4x4 transform =  trans * block_arrow_rot * glm::scale(glm::mat4x4(1.0), glm::vec3(0.03*distance));

            glm::vec3 collision_point(0), normal(0);
            if(rayIntersectsMesh(&block_arrow_mesh, transform, camera, out_origin, out_direction, collision_point, normal)){
                float test_depth = glm::length(collision_point - camera.position);
                if(test_depth < depth){
                    depth = test_depth;
                    selection_offset = collision_point - pos;
                    selected_arrow = i;
                }
            }
        }
    }
    const bool active = selected_arrow != -1;

    static float direction_offset;
    if(active){
        auto direction = axis[selected_arrow];
        
        float camera_t, direction_t;
        closestDistanceBetweenLines(out_origin, out_direction, pos + selection_offset, direction, camera_t, direction_t);
        
        glm::fvec3 unrotated_axis(selected_arrow==0, selected_arrow==1, selected_arrow==2);
        if(controls::left_mouse_click_press)
            direction_offset = -scl[selected_arrow][selected_arrow]-direction_t;
        auto scale = -(direction_t + direction_offset + scl[selected_arrow][selected_arrow])*unrotated_axis;
        if(do_snap){
            scale.x = glm::round(scale.x / snap.x) * snap.x;
            scale.y = glm::round(scale.y / snap.y) * snap.y;
            scale.z = glm::round(scale.z / snap.z) * snap.z;
        }
        scl[0][0] += scale.x;
        scl[1][1] += scale.y;
        scl[2][2] += scale.z;
    }

    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);
    
    for (int i = 0; i < 3; ++i) {
        glm::fvec3 color(i==0, i==1, i==2);
        auto direction = axis[i];

        if(selected_arrow == i)
            drawEditor3DArrow(pos, direction, camera, glm::vec4(color,1.0), glm::vec3(distance*0.03), false, true);
        else
            drawEditor3DArrow(pos, direction, camera, glm::vec4(color,0.5), glm::vec3(distance*0.03), false, true);

    }

    return active;
}


void drawWaterDebug(WaterEntity* w, const Camera &camera, bool flash = false){
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH);
    // Displays in renderdoc texture view but not in application?
    //glLineWidth(200.0);

    glUseProgram(shader::debug.program);
    auto mvp = camera.projection * camera.view * createModelMatrix(w->position, glm::quat(), w->scale);
    glUniformMatrix4fv(shader::debug.uniform("mvp"), 1, GL_FALSE, &mvp[0][0]);
    glUniformMatrix4fv(shader::debug.uniform("model"), 1, GL_FALSE, &w->position[0]);
    glUniform4f(shader::debug.uniform("color"), 1.0, 1.0, 1.0, 1.0);
    glUniform4f(shader::debug.uniform("color_flash_to"), 1.0, 0.0, 1.0, 1.0);
    glUniform1f(shader::debug.uniform("time"), glfwGetTime());
    glUniform1f(shader::debug.uniform("shaded"), 0.0);
    glUniform1f(shader::debug.uniform("flashing"), flash ? 1.0: 0.0);

    glBindVertexArray(graphics::water_grid.vao);
    glDrawElements(graphics::water_grid.draw_mode, graphics::water_grid.draw_count[0], graphics::water_grid.draw_type, (GLvoid*)(sizeof(GLubyte)*graphics::water_grid.draw_start[0]));
   
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void drawMeshCube(const glm::vec3 &pos, const glm::quat &rot, const glm::mat3x3 &scl, const Camera &camera){
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH);
    // Displays in renderdoc texture view but not in application?
    //glLineWidth(200.0);

    glUseProgram(shader::debug.program);
    auto mvp = camera.projection * camera.view * createModelMatrix(pos, rot, scl);
    glUniformMatrix4fv(shader::debug.uniform("mvp"), 1, GL_FALSE, &mvp[0][0]);
    glUniformMatrix4fv(shader::debug.uniform("model"), 1, GL_FALSE, &pos[0]);
    glUniform4f(shader::debug.uniform("color"), 1.0, 1.0, 1.0, 1.0);
    glUniform4f(shader::debug.uniform("color_flash_to"), 1.0, 0.0, 1.0, 1.0);
    glUniform1f(shader::debug.uniform("time"), glfwGetTime());
    glUniform1f(shader::debug.uniform("shaded"), 0.0);
    glUniform1f(shader::debug.uniform("flashing"), 0.0);

    drawCube();

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void drawFrustrum(Camera &drawn_camera, const Camera& camera) {
    glUseProgram(shader::debug.program);
    // Transform into drawn camera's view space, then into world space
    auto projection = glm::perspective(drawn_camera.fov, (float)window_width / (float)window_height, drawn_camera.near_plane, 3.0f*drawn_camera.near_plane);
    auto model = glm::inverse(projection*drawn_camera.view);
    auto mvp = camera.projection * camera.view * model;
    glUniformMatrix4fv(shader::debug.uniform("mvp"), 1, GL_FALSE, &mvp[0][0]);
    glUniformMatrix4fv(shader::debug.uniform("model"), 1, GL_FALSE, &model[0][0]);
    glUniform4f(shader::debug.uniform("color"), 1.0, 0.0, 1.0, 0.8);
    glUniform4f(shader::debug.uniform("color_flash_to"), 1.0, 0.0, 1.0, 1.0);
    glUniform1f(shader::debug.uniform("time"), glfwGetTime());
    glUniform1f(shader::debug.uniform("shaded"), 0.0);
    glUniform1f(shader::debug.uniform("flashing"), 0.0);

    drawLineCube();
}

void drawMeshWireframe(const Mesh &mesh, const glm::mat4& g_model_rot_scl, const glm::mat4& g_model_pos, const Camera &camera, bool flash = false){
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH);
    // Displays in renderdoc texture view but not in application?
    //glLineWidth(200.0);

    glUseProgram(shader::debug.program);
    glUniform4f(shader::debug.uniform("color"), 1.0, 1.0, 1.0, 1.0);
    glUniform4f(shader::debug.uniform("color_flash_to"), 1.0, 0.0, 1.0, 1.0);
    glUniform1f(shader::debug.uniform("time"), glfwGetTime());
    glUniform1f(shader::debug.uniform("shaded"), 0.0);
    glUniform1f(shader::debug.uniform("flashing"), flash ? 1.0: 0.0);

    auto vp = camera.projection * camera.view;

    glBindVertexArray(mesh.vao);
    for (int j = 0; j < mesh.num_meshes; ++j) {
        // Since the mesh transforms encode scale this will mess up global translation so we apply translation after
        auto model = g_model_pos * mesh.transforms[j] * g_model_rot_scl;
        auto mvp = vp * model;
        glUniformMatrix4fv(shader::debug.uniform("mvp"), 1, GL_FALSE, &mvp[0][0]);
        glUniformMatrix4fv(shader::debug.uniform("model"), 1, GL_FALSE, &model[0][0]);

        glDrawElements(mesh.draw_mode, mesh.draw_count[j], mesh.draw_type, (GLvoid*)(sizeof(*mesh.indices)*mesh.draw_start[j]));
    }
    
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void drawEditor3DRing(const glm::vec3 &position, const glm::vec3 &direction, const Camera &camera, const glm::vec4 &color, const glm::vec3 &scale, bool shaded){
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    glEnable(GL_CULL_FACE);
    glEnablei(GL_BLEND, graphics::hdr_fbo);
    glEnable(GL_BLEND);

    glBlendFunci(graphics::hdr_fbo, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  

    glUseProgram(shader::debug.program);

    auto dir = glm::normalize(direction);
    // Why?
    dir.y = -dir.y;
    static const auto ring_direction = glm::vec3(0,-1,0);

    glm::mat4x4 rot;
    if(dir == -ring_direction) rot = glm::rotate(glm::mat4x4(1.0), (float)PI, glm::vec3(1,0,0));
    else                        rot = glm::mat4_cast(glm::quat(dir, ring_direction));
    auto trans = glm::translate(glm::mat4x4(1.0), position);
    auto scl   = glm::scale(glm::mat4x4(1.0), scale);
    glm::mat4x4 transform =  trans * rot * scl;

    auto mvp = camera.projection * camera.view * transform;
    glUniformMatrix4fv(shader::debug.uniform("mvp"), 1, GL_FALSE, &mvp[0][0]);
    glUniformMatrix4fv(shader::debug.uniform("model"), 1, GL_FALSE, &transform[0][0]);
    glUniform3fv(shader::debug.uniform("sun_direction"), 1, &sun_direction[0]);
    glUniform4fv(shader::debug.uniform("color"), 1, &color[0]);
    glUniform1f(shader::debug.uniform("shaded"), shaded ? 1.0 : 0.0);
    glUniform1f(shader::debug.uniform("flashing"), 0);
    glBindVertexArray(ring_mesh.vao);
    glDrawElements(ring_mesh.draw_mode, ring_mesh.draw_count[0], ring_mesh.draw_type, (GLvoid*)(sizeof(GLubyte)*ring_mesh.draw_start[0]));

    glDisable(GL_BLEND);  
}

void drawEditor3DArrow(const glm::vec3 &position, const glm::vec3 &direction, const Camera &camera, const glm::vec4 &color, const glm::vec3 &scale, bool shaded, bool block){
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    glEnable(GL_CULL_FACE);
    glEnablei(GL_BLEND, graphics::hdr_fbo);
    glEnable(GL_BLEND);

    glBlendFunci(graphics::hdr_fbo, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  

    glUseProgram(shader::debug.program);

    auto dir = glm::normalize(direction);
    // Why?
    dir.y = -dir.y;
    static const auto arrow_direction = glm::vec3(0,-1,0);

    glm::mat4x4 rot;
    if(dir == -arrow_direction) rot = glm::rotate(glm::mat4x4(1.0), (float)PI, glm::vec3(1,0,0));
    else                        rot = glm::mat4_cast(glm::quat(dir, arrow_direction));
    auto trans = glm::translate(glm::mat4x4(1.0), position);
    auto scl   = glm::scale(glm::mat4x4(1.0), scale);
    glm::mat4x4 transform =  trans * rot * scl;

    auto mvp = camera.projection * camera.view * transform;
    glUniformMatrix4fv(shader::debug.uniform("mvp"), 1, GL_FALSE, &mvp[0][0]);
    glUniformMatrix4fv(shader::debug.uniform("model"), 1, GL_FALSE, &transform[0][0]);
    glUniform3fv(shader::debug.uniform("sun_direction"), 1, &sun_direction[0]);
    glUniform4fv(shader::debug.uniform("color"), 1, &color[0]);
    glUniform1f(shader::debug.uniform("shaded"), shaded ? 1.0 : 0.0);
    glUniform1f(shader::debug.uniform("flashing"), 0);
    if(!block){
        glBindVertexArray(arrow_mesh.vao);
        glDrawElements(arrow_mesh.draw_mode, arrow_mesh.draw_count[0], arrow_mesh.draw_type, (GLvoid*)(sizeof(GLubyte)*arrow_mesh.draw_start[0]));
    } else {
        glBindVertexArray(block_arrow_mesh.vao);
        glDrawElements(block_arrow_mesh.draw_mode, block_arrow_mesh.draw_count[0], block_arrow_mesh.draw_type, (GLvoid*)(sizeof(GLubyte)*block_arrow_mesh.draw_start[0]));

    }
    glDisable(GL_BLEND);  
}

void drawColliders(const EntityManager &entity_manager, const Camera &camera) {
    glLineWidth(0.1);

    glUseProgram(shader::debug.program);
    glUniform4f(shader::debug.uniform("color"), 0.0, 1.0, 1.0, 0.2);
    glUniform1f(shader::debug.uniform("time"), glfwGetTime());
    glUniform1f(shader::debug.uniform("shaded"), 0.0);
    glUniform1f(shader::debug.uniform("flashing"), 0.0);

    for (int i = 0; i < ENTITY_COUNT; ++i) {
        auto c = (ColliderEntity*)entity_manager.entities[i];
        if (c == nullptr || !(entityInherits(c->type, COLLIDER_ENTITY))) continue;
    
        auto model = glm::translate(glm::mat4x4(1.0), c->collider_position);
        auto mvp = camera.projection * camera.view * model;
        glUniformMatrix4fv(shader::debug.uniform("mvp"), 1, GL_FALSE, &mvp[0][0]);
        glUniformMatrix4fv(shader::debug.uniform("model"), 1, GL_FALSE, &model[0][0]);
        
        drawLineCube();
    }
}

void drawEditorGui(EntityManager &entity_manager, AssetManager &asset_manager){
    if (!use_level_camera && draw_level_camera) {
        drawFrustrum(level_camera, editor_camera);
    }
    Camera *camera_ptr = &editor_camera;
    if (use_level_camera) {
        camera_ptr = &level_camera;
    }
    Camera& camera = *camera_ptr;
    
    if (editor::draw_colliders) {
        drawColliders(entity_manager, camera);
    }
    if (editor::draw_debug_wireframe) {
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
                    drawMeshWireframe(*m_e->mesh, g_model_rot_scl, g_model_pos, camera, true);
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
    {
        if(selection.ids.size()) {
            auto fe = (Entity*)entity_manager.getEntity(selection.ids[0]);

            // Just opened
            if(sidebar_pos_right == 0) {
                sidebar_open_time = glfwGetTime();
            }
            const float sidebar_w = std::min(210.0f, window_width/2.0f);
            auto mix_t = glm::smoothstep(0.0f, sidebar_open_len, (float)glfwGetTime() - sidebar_open_time);
            sidebar_pos_right = glm::mix(sidebar_pos_right, sidebar_w, mix_t);

            if(window_resized || sidebar_pos_right != sidebar_w){
                ImGui::SetNextWindowPos(ImVec2(window_width-sidebar_pos_right,0));
                ImGui::SetNextWindowSize(ImVec2(sidebar_w,window_height));
            } else {
                ImGui::SetNextWindowPos(ImVec2(window_width-sidebar_pos_right,0), ImGuiCond_Appearing);
                ImGui::SetNextWindowSize(ImVec2(sidebar_w,window_height), ImGuiCond_Appearing);
            }
            ImGui::SetNextWindowSizeConstraints(ImVec2(sidebar_w, window_height), ImVec2(window_width / 2.0, window_height));

            ImGui::Begin("###entity", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar);
            if(selection.ids.size() == 1) 
                ImGui::TextWrapped("Entity Index: %d Version: %d", selection.ids[0].i, selection.ids[0].v);
            else 
                ImGui::TextWrapped("Multiple Entities Selected");

            const float img_w = glm::min(sidebar_w - pad, 70.0f);
            static const std::vector<std::string> image_file_extensions = { ".jpg", ".png", ".bmp", ".tiff", ".tga" };

            auto button_size = ImVec2(ImGui::GetWindowWidth() / 2.0f - pad, 2.0f * pad);
            if (entityInherits(selection.type, MESH_ENTITY)) {
                auto m_e = (MeshEntity*)fe;
                TransformType edited_transform;
                if (selection.ids.size() == 1) {
                    edited_transform = editTransform(camera, m_e->position, m_e->rotation, m_e->scale);
                }
                else {
                    glm::vec3 position = m_e->position;
                    glm::quat rotation = m_e->rotation;
                    glm::mat3 scale = m_e->scale;

                    // @todo support multiple selection rotation and scale
                    edited_transform = editTransform(camera, position, rotation, scale, TransformType::POS);

                    auto offset = position - m_e->position;
                    for (const auto& id : selection.ids) {
                        auto e = (MeshEntity*)entity_manager.getEntity(id);
                        if (e == nullptr) continue;
                        e->position += offset;
                    }
                }
                editor::transform_active = edited_transform != TransformType::NONE;

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

                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetTextLineHeight());
                if (ImGui::CollapsingHeader("Materials")) {
                    //const auto create_tex_ui = [&img_w, &asset_manager] (Texture** tex, int i, std::string&& type, bool is_float = false) {
                    //    ImGui::Text("%s", type.c_str());

                    //    std::string id = type + std::to_string(i);
                    //    bool is_color = (*tex)->is_color;
                    //    if (ImGui::Checkbox(("###is_color" + type).c_str(), &is_color)) {
                    //        (*tex)->is_color = is_color;
                    //        std::cout << id << "\n";
                    //        if ((*tex)->is_color) {
                    //            (*tex) = asset_manager.getColorTexture((*tex)->color, GL_RGBA);
                    //        }
                    //    }
                    //    if ((*tex)->is_color) {
                    //        glm::vec3& col = (*tex)->color;
                    //        ImGui::SameLine();

                    //        if (is_float) {
                    //            float val = col.x;
                    //            if (ImGui::InputFloat(("###color" + id).c_str(), &val)) {
                    //                (*tex) = asset_manager.getColorTexture(glm::vec3(val), GL_RGBA);
                    //            }
                    //        }
                    //        else {
                    //            if (ImGui::ColorEdit3(("###color" + id).c_str(), &col.x)) {
                    //                // @note maybe you want more specific format
                    //                // and color picker may make many unnecessary textures
                    //                (*tex) = asset_manager.getColorTexture(col, GL_RGBA);
                    //            }
                    //        }
                    //    }
                    //    else {
                    //        void* tex_id = (void*)(intptr_t)(*tex)->id;
                    //        ImGui::SetNextItemWidth(img_w);
                    //        if (ImGui::ImageButton(tex_id, ImVec2(img_w, img_w))) {
                    //            im_file_dialog.SetPwd(exepath + "/data/textures");
                    //            sel_e_material_index = i;
                    //            im_file_dialog_type = "asset.mat.t" + type;
                    //            im_file_dialog.SetCurrentTypeFilterIndex(2);
                    //            im_file_dialog.SetTypeFilters(image_file_extensions);
                    //            im_file_dialog.Open();
                    //        }
                    //    }
                    //};
                    //for(int i = 0; i < mesh->num_materials; i++) {
                    //    auto &mat = mesh->materials[i];
                    //    char buf[128];
                    //    sprintf(buf, "Material %d", i);
                    //    if (ImGui::CollapsingHeader(buf)){
                    //        create_tex_ui(&mat.t_albedo, i,    "Albedo");
                    //        create_tex_ui(&mat.t_ambient, i,   "Ambient", true);
                    //        create_tex_ui(&mat.t_metallic, i,  "Metallic", true);
                    //        create_tex_ui(&mat.t_normal, i,    "Normal");
                    //        create_tex_ui(&mat.t_roughness, i, "Roughness", true);
                    //    }
                    //}
                    auto albedo    = m_e->albedo_mult;
                    bool albedo_chng = false;
                    auto roughness = m_e->roughness_mult;
                    bool roughness_chng = false;
                    auto ao        = m_e->ao_mult;
                    bool ao_chng = false;
                    auto metal     = m_e->metal_mult;
                    bool metal_chng = false;
                    ImGui::SetNextItemWidth(2.0*ImGui::GetWindowWidth()/3.0 - pad);
                    albedo_chng = ImGui::ColorEdit3("albedo", &albedo[0]);
                    ImGui::SetNextItemWidth(2.0*ImGui::GetWindowWidth()/3.0 - pad);
                    roughness_chng = ImGui::SliderFloat("roughness", &roughness, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_ClampOnInput);
                    ImGui::SetNextItemWidth(2.0*ImGui::GetWindowWidth()/3.0 - pad);
                    ao_chng = ImGui::SliderFloat("ao", &ao, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_ClampOnInput);
                    ImGui::SetNextItemWidth(2.0*ImGui::GetWindowWidth()/3.0 - pad);
                    metal_chng = ImGui::SliderFloat("metal", &metal, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_ClampOnInput);

                    for (const auto& id : selection.ids) {
                        auto e = (MeshEntity*)entity_manager.getEntity(id);
                        if (e == nullptr) continue;
                        if (albedo_chng) e->albedo_mult = albedo;
                        if (roughness_chng) e->roughness_mult = roughness;
                        if (ao_chng) e->ao_mult = ao;
                        if (metal_chng) e->metal_mult = metal;
                    }
                }
            }
            if (entityInherits(selection.type, WATER_ENTITY) && selection.ids.size() == 1) {
                auto w_e = (WaterEntity*)fe;
                
                glm::quat _r = glm::quat();
                editor::transform_active = editTransform(camera, w_e->position, _r, w_e->scale, TransformType::POS_SCL) != TransformType::NONE;
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
                    bindDrawWaterColliderMap(entity_manager, w_e);
                    distanceTransformWaterFbo(w_e);
                }
            }
            if (entityInherits(selection.type, ANIMATED_MESH_ENTITY) && selection.ids.size() == 1) {
                auto a_e = (AnimatedMeshEntity*)fe;

                if (a_e->animesh != NULL) {
                    std::string animation_name = a_e->animation == NULL ? "None" : a_e->animation->name;
                    if (ImGui::BeginCombo("##asset-combo", animation_name.c_str())) {
                        for(auto &p : a_e->animesh->name_animation_map){
                            bool is_selected = (&p.second == a_e->animation); 
                            if (ImGui::Selectable(p.first.c_str(), is_selected))
                                a_e->animation = &p.second;
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
                if (a_e->animation != NULL) {
                    ImGui::Checkbox("Draw Animated: ", &a_e->draw_animated);
                    if (ImGui::SliderFloat("Current Time: ", &a_e->current_time, 0.0f, a_e->animation->duration, "%.3f")) {
                        a_e->init();
                    }
                    ImGui::SliderFloat("Time Scale: ", &a_e->time_scale, -10.0f, 10.0f, "%.3f");
                    ImGui::Checkbox("Loop: ", &a_e->loop);
                    ImGui::Checkbox("Playing: ", &a_e->playing);
                }
            }

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetTextLineHeight());
            if(!(entityInherits(selection.type, WATER_ENTITY))){
                if (ImGui::Button("Duplicate", button_size)) {
                    if (camera.state == Camera::TYPE::TRACKBALL && entityInherits(selection.type, MESH_ENTITY)) {
                        auto m_e = (MeshEntity*)fe;
                        camera.target = m_e->position + translation_snap.x;
                        updateCameraView(camera);
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
            }
            if(ImGui::Button("Delete", button_size)){
                for (auto& id : selection.ids) {
                    entity_manager.deleteEntity(id);
                    if (id == entity_manager.water) {
                        entity_manager.water = NULLID;
                    }
                }
                selection.clear();
            }
            button_size.x *= 2.0;
            if (entityInherits(selection.type, MESH_ENTITY)) {
                if (ImGui::Button("Change Mesh", button_size)) {
                    im_file_dialog.SetPwd(exepath + "/data/mesh");
                    im_file_dialog_type = "changeMesh";
                    im_file_dialog.SetCurrentTypeFilterIndex(10);
                    im_file_dialog.SetTypeFilters({".mesh"});
                    im_file_dialog.Open();
                }
            }
            
            ImGui::End();
        } else {
            sidebar_pos_right = 0;
        }
    }

    // Information shared with file browser
    //static Mesh* s_mesh = nullptr;
    //{
    //    constexpr int true_win_width = 250;
    //    int win_width = true_win_width - pad;
    //    auto button_size = ImVec2(win_width, 2*pad);
    //    auto half_button_size = ImVec2(win_width / 2.f, 2*pad);

    //    ImGui::SetNextWindowPos(ImVec2(0,0));
    //    ImGui::SetNextWindowSize(ImVec2(true_win_width, window_height));
    //    ImGui::SetNextWindowSizeConstraints(ImVec2(true_win_width, window_height), ImVec2(window_width/2.f, window_height));

    //    ImGui::Begin("Global Properties", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
    //    //if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
    //    //    float distance = glm::length(camera.position - camera.target);
    //    //    if (ImGui::SliderFloat("Distance", &distance, 1.f, 100.f)) {
    //    //        camera.position = camera.target + glm::normalize(camera.position - camera.target)*distance;
    //    //        updateCameraView(camera);
    //    //    }
    //    //}

    //    if (ImGui::CollapsingHeader("Levels")){
    //        static char level_name[256] = "";
    //        if (ImGui::Button("Save Level", half_button_size)){
    //            im_file_dialog_type = "saveLevel";
    //            im_file_dialog.SetPwd(exepath+"/data/levels");
    //            im_file_dialog.SetCurrentTypeFilterIndex(4);
    //            im_file_dialog.SetTypeFilters({".level"});
    //            im_file_dialog.Open();
    //        }
    //        ImGui::SameLine();
    //        if (ImGui::Button("Load Level", half_button_size)){
    //            im_file_dialog_type = "loadLevel";
    //            im_file_dialog.SetPwd(exepath+"/data/levels");
    //            im_file_dialog.SetCurrentTypeFilterIndex(4);
    //            im_file_dialog.SetTypeFilters({ ".level" });
    //            im_file_dialog.Open();
    //        }
    //        if (ImGui::Button("Clear Level", button_size)) {
    //            entity_manager.clear();
    //            asset_manager.clear();
    //        }
    //    }
    //    if (ImGui::CollapsingHeader("Meshes", ImGuiTreeNodeFlags_DefaultOpen)){
    //        auto &mmap = asset_manager.handle_mesh_map;
    //        if (mmap.size() > 0){
    //            if(s_mesh == nullptr) s_mesh = &mmap.begin()->second;

    //            ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - pad);
    //            if (ImGui::BeginCombo("##asset-combo", s_mesh->handle.c_str())){
    //                for(auto &a : mmap){
    //                    bool is_selected = (s_mesh == &a.second); 
    //                    if (ImGui::Selectable(a.first.c_str(), is_selected))
    //                        s_mesh = &a.second;
    //                    if (is_selected)
    //                        ImGui::SetItemDefaultFocus(); 
    //                }
    //                ImGui::EndCombo();
    //            }

    //            if(s_mesh != nullptr) {
    //                if(ImGui::Button("Add Instance", button_size)){
    //                    auto e = new MeshEntity();
    //                    e->mesh = s_mesh;
    //                    entity_manager.setEntity(entity_manager.getFreeId().i, e);
    //                }
    //                if(ImGui::Button("Export Mesh", button_size)){
    //                    im_file_dialog_type = "exportMesh";
    //                    im_file_dialog.SetPwd(exepath+"/data/models");
    //                    im_file_dialog.SetCurrentTypeFilterIndex(1);
    //                    im_file_dialog.SetTypeFilters({ ".mesh" });
    //                    im_file_dialog.Open();
    //                }
    //            }
    //        }
    //        if(ImGui::Button("Load Mesh", button_size)){
    //            im_file_dialog_type = "loadMesh";
    //            im_file_dialog.SetPwd(exepath+"/data/models");
    //            im_file_dialog.SetCurrentTypeFilterIndex(1);
    //            im_file_dialog.SetTypeFilters({ ".mesh" });
    //            im_file_dialog.Open();
    //        }
    //        if(ImGui::Button("Load Model (Assimp)", button_size)){
    //            im_file_dialog_type = "loadModelAssimp";
    //            im_file_dialog.SetPwd(exepath+"/data/models");
    //            im_file_dialog.SetCurrentTypeFilterIndex(3);
    //            im_file_dialog.SetTypeFilters({ ".obj", ".fbx" });
    //            im_file_dialog.Open();
    //        }
    //    }
    //    if (ImGui::CollapsingHeader("Graphics", ImGuiTreeNodeFlags_DefaultOpen)){
    //        if (ImGui::Checkbox("Bloom", &graphics::do_bloom)){
    //            initHdrFbo();
    //            initBloomFbo();
    //        }

    //        ImGui::Text("Sun Color:");
    //        ImGui::SetNextItemWidth(win_width);
    //        ImGui::ColorEdit3("", (float*)&sun_color); // Edit 3 floats representing a color
    //        ImGui::Text("Sun Direction:");
    //        ImGui::SetNextItemWidth(win_width);
    //        if(ImGui::InputFloat3("", (float*)&sun_direction)){
    //            sun_direction = glm::normalize(sun_direction);
    //            // Casts shadows from sun direction
    //            updateShadowVP(camera);
    //        }
    //    }
    //    ImGui::End();
    //}
    
    {
        ImGui::SetNextWindowPos(ImVec2(0,0));
        ImGui::SetNextWindowSize(ImVec2(window_width, window_height));
        ImGui::Begin("Perf Counter", NULL, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration);
        ImGui::TextWrapped("%s\n%.3f ms/frame (%.1f FPS)", 
                GL_renderer.c_str(),1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255,0,0,255));
        if(level_path != "")
            ImGui::TextWrapped("%s\n", level_path.c_str());
        if (editor::use_level_camera)
            ImGui::TextWrapped("Editing level camera!\n");
        ImGui::PopStyleColor();

        ImGui::End();
    }

    ImTerminal(entity_manager, asset_manager, do_terminal);

    // Handle imfile dialog browser
    {
        im_file_dialog.Display();
        if(im_file_dialog.HasSelected())
        {
            auto p = im_file_dialog.GetSelected().string().erase(0, exepath.length() + 1);
            std::replace(p.begin(), p.end(), '\\', '/');
            std::cout << "Selected filename at path " << p << ".\n";
            if(im_file_dialog_type == "loadLevel"){
                // @note accumulates assets
                if(loadLevel(entity_manager, asset_manager, p, level_camera)){
                    editor_camera = level_camera;
                    selection.clear();
                }
            } else if(im_file_dialog_type == "saveLevel"){
                saveLevel(entity_manager, p, level_camera);
            /*} else if(im_file_dialog_type == "exportMesh"){
                asset_manager.writeMeshFile(s_mesh, p);
            } else if(im_file_dialog_type == "loadMesh"){
                auto mesh = asset_manager.createMesh(p);
                asset_manager.loadMeshFile(mesh, p);
            */
            }else if (im_file_dialog_type == "changeMesh") {
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
            //} else if(startsWith(im_file_dialog_type, "asset.mat.t")) {
            //    if (sel_e != nullptr && sel_e->type == MESH_ENTITY) {
            //        auto m_e = static_cast<MeshEntity*>(sel_e);
            //        auto &mat = m_e->mesh->materials[sel_e_material_index];

            //        // Assets might already been loaded so just use it
            //        auto tex = asset_manager.getTexture(p);
            //        if(tex == nullptr) {
            //            tex = asset_manager.createTexture(p);
            //            asset_manager.loadTexture(tex, p);
            //        } 

            //        if(tex != nullptr) {
            //            if(endsWith(im_file_dialog_type, "Albedo")) {
            //                mat.t_albedo = tex;
            //            } else if(endsWith(im_file_dialog_type, "Ambient")) {
            //                mat.t_ambient = tex;
            //            } else if(endsWith(im_file_dialog_type, "Normal")) {
            //                mat.t_normal = tex;
            //            } else if(endsWith(im_file_dialog_type, "Metallic")) {
            //                mat.t_metallic = tex;
            //            } else if(endsWith(im_file_dialog_type, "Roughness")) {
            //                mat.t_roughness = tex;
            //            }
            //        }
            //    }
            } else {
                std::cerr << "Unhandled imgui file dialog type at path " + p + ".\n";
            }

            im_file_dialog.ClearSelected();
        }
    }

    // Rendering ImGUI
    ImGui::Render();
    auto &io = ImGui::GetIO();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void drawGameGui(EntityManager& entity_manager, AssetManager& asset_manager) {
    // Start the Dear ImGui frame;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(window_width, window_height));
        ImGui::Begin("Perf Counter", NULL, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration);
        ImGui::TextWrapped("%s\n%.3f ms/frame (%.1f FPS)",
            GL_renderer.c_str(), 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
        ImGui::TextWrapped("%s\n", level_path.c_str());
        ImGui::PopStyleColor();

        ImGui::End();
    }

    ImTerminal(entity_manager, asset_manager, do_terminal);

    // Rendering ImGUI
    ImGui::Render();
    auto& io = ImGui::GetIO();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
