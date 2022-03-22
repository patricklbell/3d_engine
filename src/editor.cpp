#include "editor.hpp"

#include <stack>

// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <GLFW/glfw3.h>

// Include ImGui
#include "glm/gtc/quaternion.hpp"
#include "imgui.h"
#include "ImGuizmo.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imfilebrowser.hpp"

#include <btBulletDynamicsCommon.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "globals.hpp"
#include "graphics.hpp"
#include "texture.hpp"

namespace editor {
    std::string im_file_dialog_type;
    GLDebugDrawer bt_debug_drawer;
    ImGui::FileBrowser im_file_dialog;
}
using namespace editor;

void loadEditorGui(){
    // Background
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    
    // create a file browser instance
    im_file_dialog_type = "";

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version.c_str());

	// Setup bullet debug renderer
    bt_debug_drawer.init();
    dynamics_world->setDebugDrawer(&bt_debug_drawer);
	bt_debug_drawer.setDebugMode(1);
}

bool editTransform(const Camera &camera, float* matrix){
    static ImGuizmo::OPERATION current_guizmo_operation(ImGuizmo::TRANSLATE);
    static ImGuizmo::MODE current_guizmo_mode(ImGuizmo::LOCAL);
    static bool use_snap = false;
    static float snap[3] = { 1.f, 1.f, 1.f };
    static float bounds[] = { -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };
    static float bound_snap[] = { 0.1f, 0.1f, 0.1f };
    static bool bound_sizing = false;
    static bool bound_sizing_snap = false;
    bool change_occured = false;

    if (ImGui::IsKeyPressed(90))
        current_guizmo_operation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(69))
        current_guizmo_operation = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed(82)) // r Key
        current_guizmo_operation = ImGuizmo::SCALE;
    if (ImGui::RadioButton("Translate", current_guizmo_operation == ImGuizmo::TRANSLATE))
        current_guizmo_operation = ImGuizmo::TRANSLATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", current_guizmo_operation == ImGuizmo::ROTATE))
        current_guizmo_operation = ImGuizmo::ROTATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", current_guizmo_operation == ImGuizmo::SCALE))
        current_guizmo_operation = ImGuizmo::SCALE;
    float matrix_translation[3], matrix_rotation[3], matrix_scale[3];
    ImGuizmo::DecomposeMatrixToComponents(matrix, matrix_translation, matrix_rotation, matrix_scale);
    if(ImGui::InputFloat3("Tr", matrix_translation)) change_occured = true;
    if(ImGui::InputFloat3("Rt", matrix_rotation)) change_occured = true;
    if(ImGui::InputFloat3("Sc", matrix_scale)) change_occured = true;
    ImGuizmo::RecomposeMatrixFromComponents(matrix_translation, matrix_rotation, matrix_scale, matrix);

    if (ImGui::IsKeyPressed(83))
        use_snap = !use_snap;
    ImGui::Checkbox("", &use_snap);
    ImGui::SameLine();

    switch (current_guizmo_operation)
    {
    case ImGuizmo::TRANSLATE:
        ImGui::InputFloat3("Snap", &snap[0]);
        break;
    case ImGuizmo::ROTATE:
        ImGui::InputFloat("Angle Snap", &snap[0]);
        break;
    case ImGuizmo::SCALE:
        ImGui::InputFloat("Scale Snap", &snap[0]);
        break;
    default:
        break;
    }
    ImGui::Checkbox("Bound Sizing", &bound_sizing);
    if (bound_sizing)
    {
        ImGui::PushID(3);
        ImGui::Checkbox("", &bound_sizing_snap);
        ImGui::SameLine();
        ImGui::InputFloat3("Snap", bound_snap);
        ImGui::PopID();
    }

    ImGuiIO& io = ImGui::GetIO();
    float viewManipulateRight = io.DisplaySize.x;
    float viewManipulateTop = 0;
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

    ImGuizmo::Manipulate(&camera.view[0][0], &camera.projection[0][0], current_guizmo_operation, current_guizmo_mode, matrix, NULL, use_snap ? &snap[0] : NULL, bound_sizing ? bounds : NULL, bound_sizing_snap ? bound_snap : NULL);
    return change_occured || ImGuizmo::IsUsing();
}

void drawEditorGui(Camera &camera, Entity *entities[ENTITY_COUNT], std::vector<ModelAsset *> &assets, std::stack<int> &free_entity_stack, std::stack<int> &delete_entity_stack, int &id_counter, btRigidBody *rigidbodies[ENTITY_COUNT], btRigidBody::btRigidBodyConstructionInfo rigidbody_CI, GBuffer &gb){

    // @->todo Batch debug rendering
    bt_debug_drawer.setMVP(camera.projection * camera.view);
    dynamics_world->debugDrawWorld();


    // Start the Dear ImGui frame;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuizmo::SetOrthographic(true);
    ImGuizmo::BeginFrame();
    {
        if(selected_entity != -1) {
            ImGui::Begin("Model Properties");

            editTransform(camera, (float *)&entities[selected_entity]->transform[0][0]);
 
            if(camera.state == Camera::TYPE::TRACKBALL){
                camera.target = glm::vec3(entities[selected_entity]->transform[3]);
            }

            // &todo Edit rigidbody inplace
            dynamics_world->removeRigidBody(rigidbodies[selected_entity]);
           
            btTransform transform;
            transform.setFromOpenGLMatrix((float*)&entities[selected_entity]->transform[0][0]);

            rigidbodies[selected_entity]->setMotionState(new btDefaultMotionState(transform));
            dynamics_world->addRigidBody(rigidbodies[selected_entity]);
            
            if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::ColorEdit3("Albedo Color", entities[selected_entity]->asset->mat->albedo);
                ImGui::ColorEdit3("Diffuse Color", entities[selected_entity]->asset->mat->diffuse);
                ImGui::InputFloat("Specular Color", &entities[selected_entity]->asset->mat->optic_density);
                ImGui::InputFloat("Reflection Sharpness", &entities[selected_entity]->asset->mat->reflect_sharp);
                ImGui::InputFloat("Specular Exponent (size)", &entities[selected_entity]->asset->mat->spec_exp);
                ImGui::InputFloat("Dissolve", &entities[selected_entity]->asset->mat->dissolve);
                ImGui::ColorEdit3("Transmission Filter", entities[selected_entity]->asset->mat->trans_filter);
                
                void * texDiffuse = (void *)(intptr_t)entities[selected_entity]->asset->mat->t_diffuse;
                if(ImGui::ImageButton(texDiffuse, ImVec2(128,128))){
                    im_file_dialog_type = "asset.mat.tDiffuse";
                    im_file_dialog.SetTypeFilters({ ".bmp" });
                    im_file_dialog.Open();
                }

                ImGui::SameLine();

                void * texNormal = (void *)(intptr_t)entities[selected_entity]->asset->mat->t_normal;
                if(ImGui::ImageButton(texNormal, ImVec2(128,128))){
                    im_file_dialog_type = "asset.mat.tNormal";
                    im_file_dialog.SetTypeFilters({ ".bmp" });
                    im_file_dialog.Open();
                }
            }
            if(ImGui::Button("Duplicate")){
                int id;
                if(free_entity_stack.size() == 0){
                    id = id_counter++;
                } else {
                    id = free_entity_stack.top();
                    free_entity_stack.pop();
                }
                entities[id] = new Entity();
                entities[id]->id = id;
                entities[id]->asset = entities[selected_entity]->asset;
                entities[id]->transform = entities[selected_entity]->transform * glm::translate(glm::mat4(), glm::vec3(0.1));
                if(rigidbodies[selected_entity] != nullptr){
                    rigidbodies[id] = new btRigidBody(
                        1/rigidbodies[selected_entity]->getInvMass(),
                        rigidbodies[selected_entity]->getMotionState(),
                        rigidbodies[selected_entity]->getCollisionShape(),
                        btVector3(0,0,0)
                    );
                    rigidbodies[id]->translate(btVector3(0.1, 0.1, 0.1));
                    dynamics_world->addRigidBody(rigidbodies[id]);
                    rigidbodies[id]->setUserIndex(id);
                }
                if(camera.state == Camera::TYPE::TRACKBALL){
                    selected_entity = id;
                    camera.target = glm::vec3(entities[selected_entity]->transform[3]);
                    updateCameraView(camera);
                }
            }
            ImGui::SameLine();
            if(ImGui::Button("Delete")){
                delete_entity_stack.push(selected_entity);
                selected_entity = -1;
            }
            ImGui::End();
        }
    }
    {
        ImGui::Begin("Global Properties");
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            float distance = glm::length(camera.position - camera.target);
            if (ImGui::SliderFloat("Distance", &distance, 1.f, 100.f)) {
                camera.position = camera.target + glm::normalize(camera.position - camera.target)*distance;
                updateCameraView(camera);
            }
        }
        if (ImGui::CollapsingHeader("Add Entity", ImGuiTreeNodeFlags_DefaultOpen)){
            static int asset_current = 0;

            if (ImGui::BeginCombo("##asset-combo", assets[asset_current]->name.c_str())){
                for (int n = 0; n < assets.size(); n++)
                {
                    bool is_selected = (asset_current == n); // You can store your selection however you want, outside or inside your objects
                    if (ImGui::Selectable(assets[n]->name.c_str(), is_selected))
                        asset_current = n;
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();   // You may set the initial focus when opening the combo (scrolling + for keyboard navigation support)
                }
                ImGui::EndCombo();
            }

            if(ImGui::Button("Add Instance")){
                int id;
                if(free_entity_stack.size() == 0){
                    id = id_counter++;
                } else {
                    id = free_entity_stack.top();
                    free_entity_stack.pop();
                }
                entities[id] = new Entity();
                entities[id]->id = id;
                entities[id]->asset = assets[asset_current];
                entities[id]->transform = glm::mat4();

                // TODO: Integrate collider with asset loading
                rigidbodies[id] = new btRigidBody(rigidbody_CI);
                rigidbodies[id]->setMotionState(new btDefaultMotionState(btTransform(btQuaternion(0,0,0),btVector3(0, 0, 0))));
                dynamics_world->addRigidBody(rigidbodies[id]);
                rigidbodies[id]->setUserIndex(id);
                if(camera.state == Camera::TYPE::TRACKBALL){
                    selected_entity = id;
                    camera.target = glm::vec3(entities[selected_entity]->transform[3]);
                    camera.view = glm::lookAt(camera.position, camera.target, camera.up);
                }
            }
        }
        
        ImGui::ColorEdit3("Clear color", (float*)&clear_color); // Edit 3 floats representing a color
        ImGui::ColorEdit3("Sun color", (float*)&sun_color); // Edit 3 floats representing a color
        if(ImGui::InputFloat3("Sun direction", (float*)&sun_direction))
            sun_direction = glm::normalize(sun_direction);
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Menu"))
            {
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();

        ImGui::Begin("GBuffer");
        {
            ImGui::Image((void *)(intptr_t)gb.textures[GBuffer::GBUFFER_TEXTURE_TYPE_DIFFUSE], ImVec2((int)window_width/10, (int)window_height/10), ImVec2(0, 1), ImVec2(1, 0));
            ImGui::SameLine();
            ImGui::Image((void *)(intptr_t)gb.textures[GBuffer::GBUFFER_TEXTURE_TYPE_NORMAL], ImVec2((int)window_width/10, (int)window_height/10), ImVec2(0, 1), ImVec2(1, 0));
            ImGui::SameLine();
            ImGui::Image((void *)(intptr_t)gb.textures[GBuffer::GBUFFER_TEXTURE_TYPE_POSITION], ImVec2((int)window_width/10, (int)window_height/10), ImVec2(0, 1), ImVec2(1, 0)) ;
        }
        ImGui::End();
        im_file_dialog.Display();
        if(im_file_dialog.HasSelected())
        {
            std::cout << "Selected filename: " << im_file_dialog.GetSelected().string() << std::endl;
            if(im_file_dialog_type == "asset.mat.tDiffuse"){
                if(selected_entity != -1)
                    entities[selected_entity]->asset->mat->t_diffuse = loadImage(im_file_dialog.GetSelected().string());
            }
            else if(im_file_dialog_type == "asset.mat.tNormal"){
                if(selected_entity != -1)
                    entities[selected_entity]->asset->mat->t_normal = loadImage(im_file_dialog.GetSelected().string());
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
