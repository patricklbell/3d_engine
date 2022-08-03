#include "editor.hpp"

#include <stack>
#include <limits>

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
#include "ImGuizmo.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imfilebrowser.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <string>

#include "globals.hpp"
#include "utilities.hpp"
#include "graphics.hpp"
#include "shader.hpp"
#include "texture.hpp"
#include "assets.hpp"
#include "controls.hpp"
#include "entities.hpp"

namespace editor {
    std::string im_file_dialog_type;
    bool draw_debug_wireframe = true;
    bool transform_active = false;
    GizmoMode gizmo_mode = GIZMO_MODE_NONE;
    ImGui::FileBrowser im_file_dialog(ImGuiFileBrowserFlags_EnterNewFilename | ImGuiFileBrowserFlags_NoTitleBar);
    Mesh arrow_mesh;
    Mesh block_arrow_mesh;
    Mesh ring_mesh;
    Id sel_e(-1, -1);
    std::map<std::string, Asset*> editor_assets;
}
using namespace editor;

void initEditorGui(){
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
    //bt_debug_drawer.init();
    //dynamics_world->setDebugDrawer(&bt_debug_drawer);
	//bt_debug_drawer.setDebugMode(1);
    loadMesh(arrow_mesh, "data/models/arrow.obj", editor_assets);
    loadMesh(block_arrow_mesh, "data/models/block_arrow.obj", editor_assets);
    loadMesh(ring_mesh, "data/models/ring.obj", editor_assets);
}

bool editTransform(Camera &camera, glm::vec3 &pos, glm::quat &rot, glm::mat3 &scl, TransformType type=TransformType::ALL){
    static bool key_t = false, key_r = false, key_s = false, key_n = false;
    static bool use_snap = false;
    static glm::vec3 snap = glm::vec3( 1.f, 1.f, 1.f );
    bool change_occured = false;

    bool do_p = (bool)((unsigned int)type & (unsigned int)TransformType::POS);
    bool do_r = (bool)((unsigned int)type & (unsigned int)TransformType::ROT);
    bool do_s = (bool)((unsigned int)type & (unsigned int)TransformType::SCL);

    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureKeyboard && glfwGetKey(window, GLFW_KEY_T) && !key_t && do_p)
        gizmo_mode = gizmo_mode == GIZMO_MODE_TRANSLATE ? GIZMO_MODE_NONE : GIZMO_MODE_TRANSLATE;
    if (!io.WantCaptureKeyboard && glfwGetKey(window, GLFW_KEY_R) && !key_r && do_r)
        gizmo_mode = gizmo_mode == GIZMO_MODE_ROTATE ? GIZMO_MODE_NONE : GIZMO_MODE_ROTATE;
    if (!io.WantCaptureKeyboard && glfwGetKey(window, GLFW_KEY_S) && !key_s && do_s)
        gizmo_mode = gizmo_mode == GIZMO_MODE_SCALE ? GIZMO_MODE_NONE : GIZMO_MODE_SCALE;

    if(do_p){
        if (ImGui::RadioButton("##translate", gizmo_mode == GIZMO_MODE_TRANSLATE)){
            if(gizmo_mode == GIZMO_MODE_TRANSLATE) gizmo_mode = GIZMO_MODE_NONE;
            else                                   gizmo_mode = GIZMO_MODE_TRANSLATE;
        }  
        ImGui::SameLine();
        ImGui::TextWrapped("Translation (Vector 3)");
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 10);
        if(ImGui::InputFloat3("##translation", &pos[0])){
            change_occured = true;
        } 
    }
    if(do_r){
        if (ImGui::RadioButton("##rotate", gizmo_mode == GIZMO_MODE_ROTATE)){
            if(gizmo_mode == GIZMO_MODE_ROTATE) gizmo_mode = GIZMO_MODE_NONE;
            else                                gizmo_mode = GIZMO_MODE_ROTATE;
        }
        ImGui::SameLine();
        ImGui::TextWrapped("Rotation (Quaternion)");
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 10);
        if(ImGui::InputFloat4("##rotation", &rot[0])){
            change_occured = true;
        } 
    }
    if(do_s){
        if (ImGui::RadioButton("##scale", gizmo_mode == GIZMO_MODE_SCALE)){
            if(gizmo_mode == GIZMO_MODE_SCALE) gizmo_mode = GIZMO_MODE_NONE;
            else                               gizmo_mode = GIZMO_MODE_SCALE;
        }
        ImGui::SameLine();
        ImGui::TextWrapped("Scale (Vector 3)");
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 10);
        glm::vec3 scale = glm::vec3(scl[0][0], scl[1][1], scl[2][2]);
        if(ImGui::InputFloat3("##scale", &scale[0])){
            scl[0][0] = scale.x;
            scl[1][1] = scale.y;
            scl[2][2] = scale.z;
            change_occured = true;
        } 
    }
  
    switch (gizmo_mode)
    {
    case GIZMO_MODE_TRANSLATE:
        if (!io.WantCaptureKeyboard && glfwGetKey(window, GLFW_KEY_N) && !key_n)
            use_snap = !use_snap;
        ImGui::Checkbox("", &use_snap);
        ImGui::SameLine();
        ImGui::InputFloat3("Snap", &snap[0]);
        change_occured |= editorTranslationGizmo(pos, rot, scl, camera, snap, use_snap);
        break;
    case GIZMO_MODE_ROTATE:
        change_occured |= editorRotationGizmo(pos, rot, scl, camera);
        break;
    case GIZMO_MODE_SCALE:
        if (!io.WantCaptureKeyboard && glfwGetKey(window, GLFW_KEY_N) && !key_n)
            use_snap = !use_snap;
        ImGui::Checkbox("", &use_snap);
        ImGui::SameLine();
        ImGui::InputFloat3("Snap", &snap[0]);
        change_occured |= editorScalingGizmo(pos, rot, scl, camera, snap, use_snap);
        break;
    default:
        break;
    }

    key_t = glfwGetKey(window, GLFW_KEY_T);
    key_r = glfwGetKey(window, GLFW_KEY_R);
    key_s = glfwGetKey(window, GLFW_KEY_S);
    key_n = glfwGetKey(window, GLFW_KEY_N);
    return change_occured;
}

bool editorRotationGizmo(glm::vec3 &pos, glm::quat &rot, glm::mat3 &scl, const Camera &camera){
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

        rot = glm::rotate(rot, angle, rotation_axis);

        rot_dir_prev = rot_dir;
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


void drawWaterDebug(WaterEntity* w_e, const Camera &camera, bool flash = false){
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH);
    // Displays in renderdoc texture view but not in application?
    //glLineWidth(200.0);

    glUseProgram(shader::debug_program);
    auto mvp = camera.projection * camera.view * createModelMatrix(w_e->position, glm::quat(), w_e->scale);
    glUniformMatrix4fv(shader::debug_uniforms.mvp, 1, GL_FALSE, &mvp[0][0]);
    glUniformMatrix4fv(shader::debug_uniforms.model, 1, GL_FALSE, &w_e->position[0]);
    glUniform4f(shader::debug_uniforms.color, 1.0, 1.0, 1.0, 1.0);
    glUniform4f(shader::debug_uniforms.color_flash_to, 1.0, 0.0, 1.0, 1.0);
    glUniform1f(shader::debug_uniforms.time, glfwGetTime());
    glUniform1f(shader::debug_uniforms.shaded, 0.0);
    glUniform1f(shader::debug_uniforms.flashing, flash ? 1.0: 0.0);

    glBindVertexArray(graphics::grid.vao);
    glDrawElements(graphics::grid.draw_mode, graphics::grid.draw_count[0], graphics::grid.draw_type, (GLvoid*)(sizeof(GLubyte)*graphics::grid.draw_start[0]));
   
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

    glUseProgram(shader::debug_program);
    auto mvp = camera.projection * camera.view * createModelMatrix(pos, rot, scl);
    glUniformMatrix4fv(shader::debug_uniforms.mvp, 1, GL_FALSE, &mvp[0][0]);
    glUniformMatrix4fv(shader::debug_uniforms.model, 1, GL_FALSE, &pos[0]);
    glUniform4f(shader::debug_uniforms.color, 1.0, 1.0, 1.0, 1.0);
    glUniform4f(shader::debug_uniforms.color_flash_to, 1.0, 0.0, 1.0, 1.0);
    glUniform1f(shader::debug_uniforms.time, glfwGetTime());
    glUniform1f(shader::debug_uniforms.shaded, 0.0);
    glUniform1f(shader::debug_uniforms.flashing, 0.0);

    drawCube();

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void drawMeshWireframe(Mesh *mesh, const glm::vec3 &pos, const glm::quat &rot, const glm::mat3x3 &scl, const Camera &camera, bool flash = false){
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH);
    // Displays in renderdoc texture view but not in application?
    //glLineWidth(200.0);

    glUseProgram(shader::debug_program);
    auto mvp = camera.projection * camera.view * createModelMatrix(pos, rot, scl);
    glUniformMatrix4fv(shader::debug_uniforms.mvp, 1, GL_FALSE, &mvp[0][0]);
    glUniformMatrix4fv(shader::debug_uniforms.model, 1, GL_FALSE, &pos[0]);
    glUniform4f(shader::debug_uniforms.color, 1.0, 1.0, 1.0, 1.0);
    glUniform4f(shader::debug_uniforms.color_flash_to, 1.0, 0.0, 1.0, 1.0);
    glUniform1f(shader::debug_uniforms.time, glfwGetTime());
    glUniform1f(shader::debug_uniforms.shaded, 0.0);
    glUniform1f(shader::debug_uniforms.flashing, flash ? 1.0: 0.0);

    for (int j = 0; j < mesh->num_materials; ++j) {
        glBindVertexArray(mesh->vao);
        glDrawElements(mesh->draw_mode, mesh->draw_count[j], mesh->draw_type, (GLvoid*)(sizeof(GLubyte)*mesh->draw_start[j]));
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

    glUseProgram(shader::debug_program);

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
    glUniformMatrix4fv(shader::debug_uniforms.mvp, 1, GL_FALSE, &mvp[0][0]);
    glUniformMatrix4fv(shader::debug_uniforms.model, 1, GL_FALSE, &transform[0][0]);
    glUniform3fv(shader::debug_uniforms.sun_direction, 1, &sun_direction[0]);
    glUniform4fv(shader::debug_uniforms.color, 1, &color[0]);
    glUniform1f(shader::debug_uniforms.shaded, shaded ? 1.0 : 0.0);
    glUniform1f(shader::debug_uniforms.flashing, 0);
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

    glUseProgram(shader::debug_program);

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
    glUniformMatrix4fv(shader::debug_uniforms.mvp, 1, GL_FALSE, &mvp[0][0]);
    glUniformMatrix4fv(shader::debug_uniforms.model, 1, GL_FALSE, &transform[0][0]);
    glUniform3fv(shader::debug_uniforms.sun_direction, 1, &sun_direction[0]);
    glUniform4fv(shader::debug_uniforms.color, 1, &color[0]);
    glUniform1f(shader::debug_uniforms.shaded, shaded ? 1.0 : 0.0);
    glUniform1f(shader::debug_uniforms.flashing, 0);
    if(!block){
        glBindVertexArray(arrow_mesh.vao);
        glDrawElements(arrow_mesh.draw_mode, arrow_mesh.draw_count[0], arrow_mesh.draw_type, (GLvoid*)(sizeof(GLubyte)*arrow_mesh.draw_start[0]));
    } else {
        glBindVertexArray(block_arrow_mesh.vao);
        glDrawElements(block_arrow_mesh.draw_mode, block_arrow_mesh.draw_count[0], block_arrow_mesh.draw_type, (GLvoid*)(sizeof(GLubyte)*block_arrow_mesh.draw_start[0]));

    }
    glDisable(GL_BLEND);  
}

void drawEditorGui(Camera &camera, EntityManager &entity_manager, std::map<std::string, Asset *> &assets){
    // 
    // Visualise entity picker and ray cast
    //
    //glm::vec3 out_origin;
    //glm::vec3 out_direction;
    //screenPosToWorldRay(controls::mouse_position, camera.view, camera.projection, out_origin, out_direction);
    //float min_collision_distance = std::numeric_limits<float>::max();
    //glm::vec3 normal, collision_point;
    //for(int i = 0; i < ENTITY_COUNT; ++i){
    //    if(entities[i] == nullptr) continue;
    //    const auto mesh = entities[i]->asset;
    //    const auto transform = entities[i]->transform;
    //    glm::vec3 collision_point_tmp(0), normal_tmp(0);
    //    rayIntersectsMesh(mesh, transform, camera, out_origin, out_direction, collision_point_tmp, normal_tmp);
    //    float dis = glm::length(collision_point_tmp - camera.position);
    //    if(dis < min_collision_distance){
    //        min_collision_distance = dis;
    //        collision_point = collision_point_tmp;
    //        normal = normal_tmp;
    //    }
    //}
    //if(min_collision_distance != std::numeric_limits<float>::max()){
    //    drawEditor3DArrow(collision_point, normal, camera, glm::vec4(1.0));
    //}
    

    // @speed
    if(sel_e.i == -1) gizmo_mode = GIZMO_MODE_NONE;
    
    // Start the Dear ImGui frame;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    {
        if(sel_e.i != -1) {
            ImGui::SetNextWindowPos(ImVec2(window_width-200,0), ImGuiCond_Appearing);
            ImGui::SetNextWindowSize(ImVec2(200,window_height), ImGuiCond_Appearing);
            if(window_resized){
                ImGui::SetNextWindowPos(ImVec2(window_width-200,0));
                ImGui::SetNextWindowSize(ImVec2(200,window_height));
            }
            ImGui::SetNextWindowSizeConstraints(ImVec2(100, window_height), ImVec2(window_width / 2.0, window_height));

            ImGui::Begin("Entity Properties", NULL, ImGuiWindowFlags_NoMove);
            ImGui::Text("Entity Index: %d Version: %d", sel_e.i, sel_e.v);

            auto s_e = entity_manager.getEntity(sel_e);
            if(s_e != nullptr){
                if(s_e->type & EntityType::MESH_ENTITY && ((MeshEntity*)s_e)->mesh != nullptr){
                    auto m_e = (MeshEntity*)s_e;
                    if(editor::draw_debug_wireframe)
                        drawMeshWireframe(m_e->mesh, m_e->position, m_e->rotation, m_e->scale, camera, true);
                    editor::transform_active = editTransform(camera, m_e->position, m_e->rotation, m_e->scale);
                } else if(s_e->type & EntityType::WATER_ENTITY) {
                    auto w_e = (WaterEntity*)s_e;
                    if(editor::draw_debug_wireframe)
                        drawWaterDebug(w_e, camera, true);
                    glm::quat _r = glm::quat();
                    editor::transform_active = editTransform(camera, w_e->position, _r, w_e->scale, TransformType::POS_SCL);
                    ImGui::TextWrapped("Shallow Color:");
                    ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 10);
                    ImGui::ColorEdit4("##shallow_color", (float*)(&w_e->shallow_color));
                    ImGui::TextWrapped("Deep Color:");
                    ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 10);
                    ImGui::ColorEdit4("##deep_color", (float*)(&w_e->deep_color));
                    ImGui::TextWrapped("Foam Color:");
                    ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 10);
                    ImGui::ColorEdit4("##foam_color", (float*)(&w_e->foam_color));
                }
            }

            //if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
            //    ImGui::ColorEdit3("Albedo Color", entities[selected_entity]->asset->mat->albedo);
            //    ImGui::ColorEdit3("Diffuse Color", entities[selected_entity]->asset->mat->diffuse);
            //    ImGui::InputFloat("Specular Color", &entities[selected_entity]->asset->mat->optic_density);
            //    ImGui::InputFloat("Reflection Sharpness", &entities[selected_entity]->asset->mat->reflect_sharp);
            //    ImGui::InputFloat("Specular Exponent (size)", &entities[selected_entity]->asset->mat->spec_exp);
            //    ImGui::InputFloat("Dissolve", &entities[selected_entity]->asset->mat->dissolve);
            //    ImGui::ColorEdit3("Transmission Filter", entities[selected_entity]->asset->mat->trans_filter);
            //    
            //    void * texDiffuse = (void *)(intptr_t)entities[selected_entity]->asset->mat->t_diffuse;
            //    if(ImGui::ImageButton(texDiffuse, ImVec2(128,128))){
            //        im_file_dialog_type = "asset.mat.tDiffuse";
            //        im_file_dialog.SetTypeFilters({ ".bmp" });
            //        im_file_dialog.Open();
            //    }

            //    ImGui::SameLine();

            //    void * texNormal = (void *)(intptr_t)entities[selected_entity]->asset->mat->t_normal;
            //    if(ImGui::ImageButton(texNormal, ImVec2(128,128))){
            //        im_file_dialog_type = "asset.mat.tNormal";
            //        im_file_dialog.SetTypeFilters({ ".bmp" });
            //        im_file_dialog.Open();
            //    }
            //}
            if(ImGui::Button("Duplicate", ImVec2(ImGui::GetWindowWidth()/2 - 5, 20))){
                if(s_e != nullptr){
                    auto e = entity_manager.duplicateEntity(sel_e);
                    if(e->type & MESH_ENTITY){
                        ((MeshEntity*)e)->position += glm::vec3(0.1);
                        if(camera.state == Camera::TYPE::TRACKBALL){
                            sel_e = e->id;
                            camera.target = ((MeshEntity*)e)->position;
                            updateCameraView(camera);
                        }
                    }
                } else {
                    entity_manager.setEntity(entity_manager.getFreeId().i, new Entity());
                }
            }
            ImGui::SameLine();
            if(ImGui::Button("Delete", ImVec2(ImGui::GetWindowWidth()/2 - 5, 20))){
                entity_manager.deleteEntity(sel_e);
                sel_e = Id(-1, -1);
            }
            ImGui::End();
        }
    }
    {
        ImGui::SetNextWindowPos(ImVec2(0,0));
        ImGui::SetNextWindowSizeConstraints(ImVec2(100, window_height), ImVec2(window_width / 2.0, window_height));

        ImGui::Begin("Global Properties", NULL, ImGuiWindowFlags_NoMove);
        //if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        //    float distance = glm::length(camera.position - camera.target);
        //    if (ImGui::SliderFloat("Distance", &distance, 1.f, 100.f)) {
        //        camera.position = camera.target + glm::normalize(camera.position - camera.target)*distance;
        //        updateCameraView(camera);
        //    }
        //}
        static char level_name[256] = "";
        if (ImGui::Button("Save Level", ImVec2(ImGui::GetWindowWidth() / 2-10, 20))){
            im_file_dialog_type = "saveLevel";
            im_file_dialog.SetTypeFilters({".level"});
            im_file_dialog.Open();
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Level", ImVec2(ImGui::GetWindowWidth() / 2-10, 20))){
            im_file_dialog_type = "loadLevel";
            im_file_dialog.SetTypeFilters({ ".level" });
            im_file_dialog.Open();
        }
        static std::string current_asset = "";
        if (assets.size() > 0 && ImGui::CollapsingHeader("Add Entity", ImGuiTreeNodeFlags_DefaultOpen)){
            if(current_asset == "") current_asset = assets.begin()->first;
            ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 10);
            if (ImGui::BeginCombo("##asset-combo", current_asset.c_str())){
                for(auto &a : assets){
                    bool is_selected = (current_asset == a.first); 
                    if (a.second->type != AssetType::MESH_ASSET) continue;
                    if (ImGui::Selectable(a.first.c_str(), is_selected))
                        current_asset = a.first;
                    if (is_selected)
                        ImGui::SetItemDefaultFocus(); 
                }
                ImGui::EndCombo();
            }
            if(ImGui::Button("Add Instance", ImVec2(ImGui::GetWindowWidth() - 10, 20))){
                auto e = new MeshEntity();
                e->mesh = &((MeshAsset*)assets[current_asset])->mesh;
                entity_manager.setEntity(entity_manager.getFreeId().i, e);
            }
            if(ImGui::Button("Export Mesh", ImVec2(ImGui::GetWindowWidth() - 10, 20))){
                im_file_dialog_type = "exportMesh";
                im_file_dialog.SetTypeFilters({ ".mesh" });
                im_file_dialog.Open();
            }
            if(ImGui::Button("Load Mesh", ImVec2(ImGui::GetWindowWidth() - 10, 20))){
                im_file_dialog_type = "loadMesh";
                im_file_dialog.SetTypeFilters({ ".mesh" });
                im_file_dialog.Open();
            }
        }

        if (ImGui::CollapsingHeader("Graphics", ImGuiTreeNodeFlags_DefaultOpen)){
            if (ImGui::Checkbox("Bloom", &shader::unified_bloom)){
                createHdrFbo();
                createBloomFbo();
            }

            ImGui::Text("Sun Color:");
            ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 10);
            ImGui::ColorEdit3("", (float*)&sun_color); // Edit 3 floats representing a color
            ImGui::Text("Sun Direction:");
            ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 10);
            if(ImGui::InputFloat3("", (float*)&sun_direction)){
                sun_direction = glm::normalize(sun_direction);
                // Casts shadows from sun direction
                updateShadowVP(camera);
            }
        }
        ImGui::SetCursorPosY(window_height - ImGui::GetTextLineHeightWithSpacing()*3);
        ImGui::TextWrapped("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();
        
        //{
        //    ImGui::Begin("GBuffers");
        //    ImGui::Image((void *)(intptr_t)graphics::shadow_buffer, ImVec2((int)1024/8, (int)1024/8), ImVec2(0, 1), ImVec2(1, 0));
        //    ImGui::End();
        //}

        im_file_dialog.Display();
        if(im_file_dialog.HasSelected())
        {
            auto p = std::string(im_file_dialog.GetSelected());
            printf("Selected filename: %s\n", im_file_dialog.GetSelected().c_str());
            if(im_file_dialog_type == "loadLevel"){
                entity_manager.reset();
                // @fix uses more memory than necessary/accumulates assets because unused assets arent destroyed
                loadLevel(entity_manager, assets, p) ;
                sel_e = Id(-1, -1);
            } else if(im_file_dialog_type == "saveLevel"){
                saveLevel(entity_manager, assets, p);
            } else if(im_file_dialog_type == "exportMesh"){
                writeMeshFile(((MeshAsset*)assets[current_asset])->mesh, p);
            } else if(im_file_dialog_type == "loadMesh"){
                auto m_a = new MeshAsset(p);
                readMeshFile(m_a->mesh, p);
                assets[p] = m_a;
            } else {
                fprintf(stderr, "Unhandled imgui file dialog type %s.\n", p.c_str());
            }

            //if(im_file_dialog_type == "asset.mat.tDiffuse"){
            //    if(selected_entity != -1)
            //        entities[selected_entity]->asset->mat->t_diffuse = loadImage(im_file_dialog.GetSelected().string());
            //}
            //else if(im_file_dialog_type == "asset.mat.tNormal"){
            //    if(selected_entity != -1)
            //        entities[selected_entity]->asset->mat->t_normal = loadImage(im_file_dialog.GetSelected().string());
            //}
            im_file_dialog.ClearSelected();
        }
    }

    // Rendering ImGUI
    ImGui::Render();
    auto &io = ImGui::GetIO();
    glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
