#include <vector>
#include <array>
#include <stdio.h>
#include <string>
#include <cstring>
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cinttypes>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

#include "assimp/material.h"
#include "assimp/types.h"
#include "texture.hpp"
#include "assets.hpp"
#include "shader.hpp"
#include "graphics.hpp"
#include "utilities.hpp"
#include "globals.hpp"


glm::mat4x4 aiMat4x4ToGlm(const aiMatrix4x4& m) {
    return glm::mat4x4(
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4
    );
}

glm::vec2 aiVec2ToGlm(const aiVector2D& v) {
    return glm::vec2(v.x, v.y);
}

glm::vec3 aiVec3ToGlm(const aiVector3D& v) {
    return glm::vec3(v.x, v.y, v.z);
}

glm::quat aiQuatToGlm(const aiQuaternion& q) {
    return glm::quat(q.w, q.x, q.y, q.z);
}

//
// --------------------------------- Materials ---------------------------------
//
Material *default_material;
void initDefaultMaterial(AssetManager &asset_manager){
    default_material = new Material;
   
    default_material->t_albedo    = asset_manager.getColorTexture(glm::vec3(1,0,1), GL_SRGB);
    default_material->t_normal    = asset_manager.getColorTexture(glm::vec3(0.5,0.5,1), GL_RGB);
    default_material->t_metallic  = asset_manager.getColorTexture(glm::vec3(0), GL_R16F);
    default_material->t_roughness = asset_manager.getColorTexture(glm::vec3(1), GL_R16F);
    default_material->t_ambient   = asset_manager.getColorTexture(glm::vec3(1), GL_R16F);

    default_material->t_albedo->handle    = "DEFAULT:albedo";
    default_material->t_normal->handle    = "DEFAULT:normal";
    default_material->t_metallic->handle  = "DEFAULT:metallic";
    default_material->t_roughness->handle = "DEFAULT:roughness";
    default_material->t_ambient->handle   = "DEFAULT:ambient";
}

std::ostream &operator<<(std::ostream &os, const Texture &t) {
    if(t.is_color) {
        os << std::string("color (") << t.color  << std::string(")");
    } else {
        os << std::string("texture (path: ") << t.handle << std::string(")");
    }
    return os;
}

std::ostream &operator<<(std::ostream &os, const Material &m) {
    if(m.t_albedo != nullptr) 
        os << std::string("\tAlbedo: ")    << *m.t_albedo << std::string("\n");
    if(m.t_ambient != nullptr) 
        os << std::string("\tAmbient: ")   << *m.t_ambient << std::string("\n");
    if(m.t_roughness != nullptr) 
        os << std::string("\tRoughness: ") << *m.t_roughness << std::string("\n");
    if(m.t_metallic != nullptr) 
        os << std::string("\tMetallic: ")  << *m.t_metallic << std::string("\n");
    if(m.t_normal != nullptr) 
        os << std::string("\tNormal: ")    << *m.t_normal;

    return os;
}

//
// ----------------------------------- Mesh ------------------------------------
//
Mesh::~Mesh(){
    glDeleteBuffers(1, &indices_vbo);
    glDeleteBuffers(1, &vertices_vbo);
    glDeleteBuffers(1, &normals_vbo);
    glDeleteBuffers(1, &tangents_vbo);
    glDeleteBuffers(1, &uvs_vbo);
    glDeleteBuffers(1, &bone_ids_vbo);
    glDeleteBuffers(1, &weights_vbo);

    glDeleteVertexArrays(1, &vao);

    free(materials);
    free(material_indices);
    free(indices);
    free(vertices);
    free(normals);
    free(tangents);
    free(uvs);
    free(bone_ids);
    free(weights);

    free(draw_start);
    free(draw_count);
}

constexpr uint16_t MESH_FILE_VERSION = 3U;
constexpr uint16_t MESH_FILE_MAGIC   = 7543U;
// For now dont worry about size of types on different platforms
bool AssetManager::writeMeshFile(const Mesh *mesh, const std::string &path){
    std::cout << "--------------------Save Mesh " << path << "--------------------\n";

    FILE *f;
    f=fopen(path.c_str(), "wb");

    if (!f) {
        std::cerr << "Error in writing mesh file to path " << path << ".\n";
        return false;
    }

    fwrite(&MESH_FILE_MAGIC  , sizeof(MESH_FILE_MAGIC  ), 1, f);
    fwrite(&MESH_FILE_VERSION, sizeof(MESH_FILE_VERSION), 1, f);

    fwrite(&mesh->num_indices, sizeof(mesh->num_indices), 1, f);
    fwrite(mesh->indices, sizeof(*mesh->indices), mesh->num_indices, f);

    fwrite(&mesh->attributes, sizeof(mesh->attributes), 1, f);

    fwrite(&mesh->num_vertices, sizeof(mesh->num_vertices), 1, f);
    if(mesh->attributes & MESH_ATTRIBUTES_VERTICES)
        fwrite(mesh->vertices, sizeof(*mesh->vertices), mesh->num_vertices, f);
    if (mesh->attributes & MESH_ATTRIBUTES_NORMALS)
        fwrite(mesh->normals , sizeof(*mesh->normals ), mesh->num_vertices, f);
    if (mesh->attributes & MESH_ATTRIBUTES_TANGENTS)
        fwrite(mesh->tangents, sizeof(*mesh->tangents), mesh->num_vertices, f);
    if (mesh->attributes & MESH_ATTRIBUTES_UVS)
        fwrite(mesh->uvs     , sizeof(*mesh->uvs     ), mesh->num_vertices, f);
    if (mesh->attributes & MESH_ATTRIBUTES_BONES) {
        fwrite(mesh->bone_ids, sizeof(*mesh->bone_ids), mesh->num_vertices, f);
        fwrite(mesh->weights , sizeof(*mesh->weights ), mesh->num_vertices, f);
    }

    // Write materials as list of image paths
    fwrite(&mesh->num_materials, sizeof(mesh->num_materials), 1, f);

    const auto write_texture = [&f](Texture* tex) {
        char is_color = tex->is_color;
        fwrite(&is_color, sizeof(is_color), 1, f);
        if (tex->is_color) {
            fwrite(&tex->color, sizeof(tex->color), 1, f);
        }
        else {
            uint8_t len = tex->handle.size();
            fwrite(&len, sizeof(len), 1, f);
            fwrite(tex->handle.data(), len, 1, f);
        }
    };
    for(int i = 0; i < mesh->num_materials; i++){
        auto &mat = mesh->materials[i];

        write_texture(mat.t_albedo);
        write_texture(mat.t_normal);
        write_texture(mat.t_ambient);
        write_texture(mat.t_metallic);
        write_texture(mat.t_roughness);
    }

    fwrite(&mesh->num_meshes, sizeof(mesh->num_meshes), 1, f);

    // Write material indice ranges
    fwrite(mesh->material_indices, sizeof(*mesh->material_indices), mesh->num_meshes, f);
    fwrite(mesh->draw_start,       sizeof(*mesh->draw_start),       mesh->num_meshes, f);
    fwrite(mesh->draw_count,       sizeof(*mesh->draw_count),       mesh->num_meshes, f);
    fwrite(mesh->transforms,       sizeof(*mesh->transforms),       mesh->num_meshes, f);

    fclose(f);
    return true;
}

static void createMeshVao(Mesh *mesh){
	glGenVertexArrays(1, &mesh->vao);
	glBindVertexArray(mesh->vao);

    if (mesh->attributes & MESH_ATTRIBUTES_VERTICES) {
	    glGenBuffers(1, &mesh->vertices_vbo);
	    glBindBuffer(GL_ARRAY_BUFFER, mesh->vertices_vbo);
	    glBufferData(GL_ARRAY_BUFFER, mesh->num_vertices * sizeof(*mesh->vertices), &mesh->vertices[0], GL_STATIC_DRAW);
	    glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, 0);
	    glEnableVertexAttribArray(0);
    }
    if (mesh->attributes & MESH_ATTRIBUTES_NORMALS) {
	    glGenBuffers(1, &mesh->normals_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, mesh->normals_vbo);
        glBufferData(GL_ARRAY_BUFFER, mesh->num_vertices * sizeof(*mesh->normals), &mesh->normals[0], GL_STATIC_DRAW);
        glVertexAttribPointer(1, 3, GL_FLOAT, false, 0, 0);
        glEnableVertexAttribArray(1);
    }
    if (mesh->attributes & MESH_ATTRIBUTES_TANGENTS) {
	    glGenBuffers(1, &mesh->tangents_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, mesh->tangents_vbo);
        glBufferData(GL_ARRAY_BUFFER, mesh->num_vertices * sizeof(*mesh->tangents), &mesh->tangents[0], GL_STATIC_DRAW);
        glVertexAttribPointer(2, 3, GL_FLOAT, false, 0, 0);
        glEnableVertexAttribArray(2);
    }
    if (mesh->attributes & MESH_ATTRIBUTES_UVS) {
	    glGenBuffers(1, &mesh->uvs_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, mesh->uvs_vbo);
        glBufferData(GL_ARRAY_BUFFER, mesh->num_vertices * sizeof(*mesh->uvs), &mesh->uvs[0], GL_STATIC_DRAW);
        glVertexAttribPointer(3, 2, GL_FLOAT, false, 0, 0);
        glEnableVertexAttribArray(3);
    }
    if (mesh->attributes & MESH_ATTRIBUTES_BONES) {
        glGenBuffers(1, &mesh->bone_ids_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, mesh->bone_ids_vbo);
        glBufferData(GL_ARRAY_BUFFER, mesh->num_vertices * sizeof(*mesh->bone_ids), &mesh->bone_ids[0], GL_STATIC_DRAW);
        glVertexAttribIPointer(5, 4, GL_INT, 0, 0);
        glEnableVertexAttribArray(5);

        glGenBuffers(1, &mesh->weights_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, mesh->weights_vbo);
        glBufferData(GL_ARRAY_BUFFER, mesh->num_vertices * sizeof(*mesh->weights), &mesh->weights[0], GL_STATIC_DRAW);
        glVertexAttribPointer(6, 4, GL_FLOAT, false, 0, 0);
        glEnableVertexAttribArray(6);
    }

	glGenBuffers(1, &mesh->indices_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->indices_vbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->num_indices * sizeof(*mesh->indices), &mesh->indices[0], GL_STATIC_DRAW);
   
	glBindVertexArray(0); // Unbind the VAO @perf
}

bool AssetManager::loadMeshFile(Mesh *mesh, const std::string &path){
    std::cout << "----------------Loading Mesh File " << path << "----------------\n";

    FILE *f;
    f=fopen(path.c_str(), "rb");
    if (!f) {
        std::cerr << "Error in reading mesh file " << path << "\n.";
        return false;
    }

    std::remove_cv_t<decltype(MESH_FILE_MAGIC)> magic;
    fread(&magic, sizeof(magic), 1, f);
    if (magic != MESH_FILE_MAGIC) {
        std::cerr << "Invalid mesh file magic, was " << magic << " expected " << MESH_FILE_MAGIC << ".\n";
        return false;
    }

    std::remove_cv_t<decltype(MESH_FILE_VERSION)> version;
    fread(&version, sizeof(version), 1, f);
    if(version!=MESH_FILE_VERSION){
        std::cerr << "Invalid mesh file version, was " << version << " expected " << MESH_FILE_VERSION << ".\n";
        return false;
    }

    fread(&mesh->num_indices, sizeof(mesh->num_indices), 1, f);
    std::cout << "Number of indices " << mesh->num_indices << ".\n";
    mesh->indices = reinterpret_cast<decltype(mesh->indices)>(malloc(sizeof(*mesh->indices)*mesh->num_indices));
    fread(mesh->indices, sizeof(*mesh->indices), mesh->num_indices, f);

    fread(&mesh->attributes, sizeof(mesh->attributes), 1, f);

    fread(&mesh->num_vertices, sizeof(mesh->num_vertices), 1, f);
    std::cout << "Number of vertices " << mesh->num_vertices << ".\n";
    // @todo allocate in one go and index into buffer
    if (mesh->attributes & MESH_ATTRIBUTES_VERTICES) {
        mesh->vertices = reinterpret_cast<decltype(mesh->vertices)>(malloc(sizeof(*mesh->vertices) * mesh->num_vertices));
        fread(mesh->vertices, sizeof(*mesh->vertices), mesh->num_vertices, f);
    }
    if (mesh->attributes& MESH_ATTRIBUTES_NORMALS) {
        mesh->normals = reinterpret_cast<decltype(mesh->normals)>(malloc(sizeof(*mesh->normals) * mesh->num_vertices));
        fread(mesh->normals, sizeof(*mesh->normals), mesh->num_vertices, f);
    }
    if (mesh->attributes & MESH_ATTRIBUTES_TANGENTS) {
        mesh->tangents = reinterpret_cast<decltype(mesh->tangents)>(malloc(sizeof(*mesh->tangents) * mesh->num_vertices));
        fread(mesh->tangents, sizeof(*mesh->tangents), mesh->num_vertices, f);
    }
    if (mesh->attributes & MESH_ATTRIBUTES_UVS) {
        mesh->uvs = reinterpret_cast<decltype(mesh->uvs)>(malloc(sizeof(*mesh->uvs) * mesh->num_vertices));
        fread(mesh->uvs, sizeof(*mesh->uvs), mesh->num_vertices, f);
    }
    if (mesh->attributes & MESH_ATTRIBUTES_BONES) {
        mesh->bone_ids = reinterpret_cast<decltype(mesh->bone_ids)>(malloc(sizeof(*mesh->bone_ids) * mesh->num_vertices));
        fread(mesh->bone_ids, sizeof(*mesh->bone_ids), mesh->num_vertices, f);

        mesh->weights = reinterpret_cast<decltype(mesh->weights)>(malloc(sizeof(*mesh->weights) * mesh->num_vertices));
        fread(mesh->weights, sizeof(*mesh->weights), mesh->num_vertices, f);
    }

    // @note this doesn't support embedded materials for binary formats that assimp loads
    fread(&mesh->num_materials, sizeof(mesh->num_materials), 1, f);
    mesh->materials = reinterpret_cast<decltype(mesh->materials)>(malloc(sizeof(*mesh->materials)*mesh->num_materials));

#if DO_MULTITHREAD
    using tpl_t = std::tuple<Texture**, ImageData*, Texture*>;
    std::vector<tpl_t> texture_imagedata_default_list;
#endif

    const auto read_texture = [&f, 
#if DO_MULTITHREAD
        &texture_imagedata_default_list, 
#endif
        this](Texture* &tex, Texture *default_tex) {
        char is_color;
        fread(&is_color, sizeof(is_color), 1, f);
        if (is_color) {
            glm::vec3 color;
            fread(&color, sizeof(color), 1, f);
            if (color.x > 1 || color.y > 1 || color.z > 1) {
                tex = this->getColorTexture(color, GL_RGB16F);
            }
            else {
                // @note this writes the color fields again
                tex = this->getColorTexture(color);
            }
        }
        else {
            uint8_t len;
            fread(&len, sizeof(len), 1, f);
            std::string path;
            path.resize(len);
            fread(&path[0], sizeof(char), len, f);

            if (path == default_tex->handle) {
                tex = default_tex;
            }
            else {
                tex = createTexture(path);
#if DO_MULTITHREAD 
                auto& tpl = texture_imagedata_default_list.emplace_back(tpl_t{ &tex, new ImageData(), default_tex });
#else
                if (!this->loadTexture(tex, path, GL_SRGB))
                    tex = default_tex;
#endif
            }
        }
    };
    for(int i = 0; i < mesh->num_materials; ++i){
        auto &mat = mesh->materials[i];

        read_texture(mat.t_albedo, default_material->t_albedo);
        read_texture(mat.t_normal, default_material->t_normal);
        read_texture(mat.t_ambient, default_material->t_ambient);
        read_texture(mat.t_metallic, default_material->t_metallic);
        read_texture(mat.t_roughness, default_material->t_roughness);

        std::cout << "Material " << i << ": \n" << mat << "\n";
    }

#if DO_MULTITHREAD 
    for (auto& tpl : texture_imagedata_default_list) {
        ImageData* img_ptr = std::get<1>(tpl);
        std::string path = (*std::get<0>(tpl))->handle;
        global_thread_pool->queueJob(std::bind(loadImageData, img_ptr, path, GL_RGB16F));
    }

    // Block main thread until texture loading is finished
    while(global_thread_pool->busy()){}

    // Now transfer loaded data into actual textures
    for (auto& tpl : texture_imagedata_default_list) {
        auto& tex     = std::get<0>(tpl);
        auto& img     = std::get<1>(tpl);
        auto& def_tex = std::get<2>(tpl);

        (*tex)->id = createGLTextureFromData(img, GL_SRGB);
        if ((*tex)->id == GL_FALSE) {
            (*tex) = def_tex;
        }
        free(img);
    }
#endif

    fread(&mesh->num_meshes, sizeof(mesh->num_meshes), 1, f);

    mesh->material_indices = reinterpret_cast<decltype(mesh->material_indices)>(malloc(sizeof(*mesh->material_indices)*mesh->num_meshes));
    mesh->draw_start       = reinterpret_cast<decltype(mesh->draw_start)      >(malloc(sizeof(*mesh->draw_start      )*mesh->num_meshes));
    mesh->draw_count       = reinterpret_cast<decltype(mesh->draw_count)      >(malloc(sizeof(*mesh->draw_count      )*mesh->num_meshes));
    mesh->transforms       = reinterpret_cast<decltype(mesh->transforms)      >(malloc(sizeof(*mesh->transforms      )*mesh->num_meshes));
    fread(mesh->material_indices, sizeof(*mesh->material_indices), mesh->num_meshes, f);
    fread(mesh->draw_start,       sizeof(*mesh->draw_start      ), mesh->num_meshes, f);
    fread(mesh->draw_count,       sizeof(*mesh->draw_count      ), mesh->num_meshes, f);
    fread(mesh->transforms,       sizeof(*mesh->transforms      ), mesh->num_meshes, f);
    
    fclose(f);

    createMeshVao(mesh);
    return true;
}

void collapseAssimpMeshScene(aiNode* node, const aiScene* scene, std::vector<aiMesh*>& meshes, std::vector<aiMatrix4x4> &ai_global_transforms, aiMatrix4x4 ai_nodes_global_transform) {
    // process all the node's meshes (if any)
    for (int i = 0; i < node->mNumMeshes; i++) {
        auto ai_mesh = scene->mMeshes[node->mMeshes[i]];
        std::cout << "Primitive type: " << ai_mesh->mPrimitiveTypes << "\n";
        if (ai_mesh->mPrimitiveTypes != aiPrimitiveType_TRIANGLE) continue;

        // @perf duplicates transform, could use indices/preserve scene graph
        ai_global_transforms.push_back(ai_nodes_global_transform);
        meshes.push_back(ai_mesh);
    }
    // then do the same for each of its children
    for (int i = 0; i < node->mNumChildren; i++) {
        auto& child = node->mChildren[i];
        auto childs_global_transform = child->mTransformation * ai_nodes_global_transform;
        collapseAssimpMeshScene(child, scene, meshes, ai_global_transforms, childs_global_transform);
    }
};

bool AssetManager::loadMeshAssimpScene(Mesh *mesh, const std::string &path, const aiScene* scene, 
                                       const std::vector<aiMesh*> &ai_meshes, const std::vector<aiMatrix4x4>& ai_meshes_global_transforms) {

    // Allocate arrays for each material 
    mesh->num_materials = scene->mNumMaterials;
    mesh->materials = reinterpret_cast<decltype(mesh->materials)>(malloc(sizeof(*mesh->materials)*mesh->num_materials));

    for (int i = 0; i < mesh->num_materials; ++i) {
        // Load material from assimp
        auto ai_mat = scene->mMaterials[i];
        auto& mat = mesh->materials[i];

        if (!loadTextureFromAssimp(mat.t_ambient, ai_mat, scene, aiTextureType_AMBIENT_OCCLUSION, GL_RGB16F)) {
            if (!loadTextureFromAssimp(mat.t_ambient, ai_mat, scene, aiTextureType_AMBIENT, GL_RGB16F)) {
                mat.t_ambient = default_material->t_ambient;
            }
        }

        if (!loadTextureFromAssimp(mat.t_albedo, ai_mat, scene, aiTextureType_BASE_COLOR, GL_RGB16F)) {
            // If base color isnt present assume diffuse is really an albedo
            if (!loadTextureFromAssimp(mat.t_albedo, ai_mat, scene, aiTextureType_DIFFUSE, GL_RGB16F)) {
                aiColor4D col(1.f, 0.f, 1.f, 1.0f);
                bool flag = true;
                if (ai_mat->Get(AI_MATKEY_BASE_COLOR, col) != AI_SUCCESS) {
                    if (ai_mat->Get(AI_MATKEY_COLOR_DIFFUSE, col) != AI_SUCCESS) {
                        mat.t_albedo = default_material->t_albedo;
                        flag = false;
                    }
                }
                if(flag) {
                    auto color = glm::vec3(col.r, col.g, col.b);

                    aiColor3D emission_color;
                    // @todo proper emissive color/texture
                    if (ai_mat->Get(AI_MATKEY_COLOR_EMISSIVE, emission_color) == AI_SUCCESS) {
                        auto ec = glm::vec3(emission_color.r, emission_color.g, emission_color.b);
                        std::cout << "Emissive color is " << ec << "\n";
                        if (glm::length(ec) > 1) {
                            color *= ec;
                        }
                    }
                    mat.t_albedo = getColorTexture(color, GL_RGB16F);
                    mat.t_albedo->is_color = true;
                    mat.t_albedo->color = color;
                }
            }
        }

        if (!loadTextureFromAssimp(mat.t_metallic, ai_mat, scene, aiTextureType_METALNESS, GL_RGB16F)) {
            if (!loadTextureFromAssimp(mat.t_metallic, ai_mat, scene, aiTextureType_REFLECTION, GL_RGB16F)) {
                if (!loadTextureFromAssimp(mat.t_metallic, ai_mat, scene, aiTextureType_SPECULAR, GL_RGB16F)) {
                    float shininess;
                    if (ai_mat->Get(AI_MATKEY_SHININESS_STRENGTH, shininess) != AI_SUCCESS) {
                        mat.t_metallic = default_material->t_metallic;
                    }
                    else {
                        shininess = glm::clamp(shininess, 0.0f, 1.0f);
                        auto color = glm::vec3(shininess);
                        mat.t_metallic = getColorTexture(color);
                        mat.t_metallic->is_color = true;
                        mat.t_metallic->color = color;
                    }
                }
            }
        }

        if (!loadTextureFromAssimp(mat.t_roughness, ai_mat, scene, aiTextureType_DIFFUSE_ROUGHNESS, GL_RGB16F)) {
            if (!loadTextureFromAssimp(mat.t_roughness, ai_mat, scene, aiTextureType_SHININESS, GL_RGB16F)) {
                float roughness;
                if (ai_mat->Get(AI_MATKEY_SHININESS, roughness) != AI_SUCCESS) {
                    mat.t_roughness = default_material->t_roughness;
                }
                else {
                    roughness = glm::clamp(roughness, 0.0f, 1.0f);
                    auto color = glm::vec3(roughness);
                    mat.t_roughness = getColorTexture(color);
                    mat.t_roughness->is_color = true;
                    mat.t_roughness->color = color;
                }
            }
        }

        // @note Since mtl files specify normals as bump maps assume all bump maps are really normals
        if (!loadTextureFromAssimp(mat.t_normal, ai_mat, scene, aiTextureType_NORMALS, GL_RGB16F)) {
            if (!loadTextureFromAssimp(mat.t_normal, ai_mat, scene, aiTextureType_HEIGHT, GL_RGB16F)) {
                mat.t_normal = default_material->t_normal;
            }
        }

        std::cout << "Material " << i << ": \n" << mat << "\n";
    }

    // Allocate arrays for each mesh 
    mesh->num_meshes = ai_meshes.size();
    std::cout << "Number of meshes is " << mesh->num_meshes << "\n";
    mesh->draw_start = reinterpret_cast<decltype(mesh->draw_start)>(malloc(sizeof(*mesh->draw_start) * mesh->num_meshes));
    mesh->draw_count = reinterpret_cast<decltype(mesh->draw_count)>(malloc(sizeof(*mesh->draw_count) * mesh->num_meshes));
    mesh->transforms = reinterpret_cast<decltype(mesh->transforms)>(malloc(sizeof(*mesh->transforms) * mesh->num_meshes));

    mesh->material_indices = reinterpret_cast<decltype(mesh->material_indices)>(malloc(sizeof(*mesh->material_indices) * mesh->num_meshes));

    mesh->attributes = (MeshAttributes)(MESH_ATTRIBUTES_VERTICES | MESH_ATTRIBUTES_NORMALS | MESH_ATTRIBUTES_TANGENTS | MESH_ATTRIBUTES_UVS);
    mesh->num_indices  = 0;
    mesh->num_vertices = 0;
	for (int i = 0; i < mesh->num_meshes; ++i) {
		const aiMesh* ai_mesh = ai_meshes[i]; 

		mesh->draw_start[i] = mesh->num_indices;
		mesh->draw_count[i] = 3*ai_mesh->mNumFaces;
        mesh->num_indices += 3*ai_mesh->mNumFaces;

        mesh->num_vertices += ai_mesh->mNumVertices;

        mesh->material_indices[i] = ai_mesh->mMaterialIndex;

        mesh->transforms[i] = aiMat4x4ToGlm(ai_meshes_global_transforms[i]);

        if (ai_mesh->mNormals == NULL) {
            mesh->attributes = (MeshAttributes)(mesh->attributes ^ MESH_ATTRIBUTES_NORMALS);
        }
        if (ai_mesh->mTangents == NULL) {
            mesh->attributes = (MeshAttributes)(mesh->attributes ^ MESH_ATTRIBUTES_TANGENTS);
        }
        if (ai_mesh->mTextureCoords[0] == NULL) {
            mesh->attributes = (MeshAttributes)(mesh->attributes ^ MESH_ATTRIBUTES_UVS);
        }
	}
    std::cout << "Mesh attributes are " << (int)mesh->attributes << "\n";
    std::cout << "Number of vertices is " << mesh->num_vertices << "\n";

    bool mallocs_failed = false;
    mesh->vertices = reinterpret_cast<decltype(mesh->vertices)>(malloc(sizeof(*mesh->vertices)*mesh->num_vertices));
    mallocs_failed |= mesh->vertices == NULL;

    if (mesh->attributes & MESH_ATTRIBUTES_NORMALS) {
        mesh->normals  = reinterpret_cast<decltype(mesh->normals )>(malloc(sizeof(*mesh->normals )*mesh->num_vertices));
        mallocs_failed |= mesh->normals == NULL;
    }
    if (mesh->attributes & MESH_ATTRIBUTES_TANGENTS) {
        mesh->tangents = reinterpret_cast<decltype(mesh->tangents)>(malloc(sizeof(*mesh->tangents)*mesh->num_vertices));
        mallocs_failed |= mesh->normals == NULL;
    }
    if (mesh->attributes & MESH_ATTRIBUTES_UVS) {
        mesh->uvs      = reinterpret_cast<decltype(mesh->uvs     )>(malloc(sizeof(*mesh->uvs     )*mesh->num_vertices));
        mallocs_failed |= mesh->uvs == NULL;
    }

    mesh->indices  = reinterpret_cast<decltype(mesh->indices )>(malloc(sizeof(*mesh->indices )*mesh->num_indices));
    mallocs_failed |= mesh->indices == NULL;

    if(mallocs_failed) {
        std::cerr << "Mallocs failed in mesh loader, freeing mesh\n";
        free(mesh->vertices);
        free(mesh->normals);
        free(mesh->tangents);
        free(mesh->uvs);
        free(mesh->indices);
        return false;
    }

    int vertices_offset = 0, indices_offset = 0;
    for (int j = 0; j < mesh->num_meshes; ++j) {
        const aiMesh* ai_mesh = ai_meshes[j];

		for(unsigned int i=0; i<ai_mesh->mNumVertices; i++){
            mesh->vertices[vertices_offset + i] = glm::fvec3(
                ai_mesh->mVertices[i].x,
                ai_mesh->mVertices[i].y,
                ai_mesh->mVertices[i].z
            );
		}
        if (mesh->attributes & MESH_ATTRIBUTES_NORMALS) {
            for(unsigned int i=0; i<ai_mesh->mNumVertices; i++){
                mesh->normals[vertices_offset + i] = glm::fvec3(
                    ai_mesh->mNormals[i].x,
                    ai_mesh->mNormals[i].y,
                    ai_mesh->mNormals[i].z
                );
            }
		}
        if (mesh->attributes & MESH_ATTRIBUTES_TANGENTS) {
            for(unsigned int i=0; i<ai_mesh->mNumVertices; i++){
                mesh->tangents[vertices_offset + i] = glm::fvec3(
                    ai_mesh->mTangents[i].x,
                    ai_mesh->mTangents[i].y,
                    ai_mesh->mTangents[i].z
                );
            }
		}
        if (mesh->attributes & MESH_ATTRIBUTES_UVS) {
            for(unsigned int i=0; i<ai_mesh->mNumVertices; i++){
                mesh->uvs[vertices_offset + i] = glm::fvec2(
                    ai_mesh->mTextureCoords[0][i].x,
                    ai_mesh->mTextureCoords[0][i].y
                );
		    }
		}
		
		for (unsigned int i=0; i<ai_mesh->mNumFaces; i++){
			// @note assumes the model has only triangles.
			mesh->indices[indices_offset + 3*i    ] = vertices_offset + ai_mesh->mFaces[i].mIndices[0];
			mesh->indices[indices_offset + 3*i + 1] = vertices_offset + ai_mesh->mFaces[i].mIndices[1];
			mesh->indices[indices_offset + 3*i + 2] = vertices_offset + ai_mesh->mFaces[i].mIndices[2];
		}

        vertices_offset += ai_mesh->mNumVertices;
        indices_offset += ai_mesh->mNumFaces*3;
    }

    return true;
}

static constexpr auto ai_import_flags = aiProcess_JoinIdenticalVertices |
aiProcess_Triangulate |
aiProcess_GenNormals |
aiProcess_CalcTangentSpace |
aiProcess_GenUVCoords |
aiProcess_FlipUVs |
//aiProcess_RemoveComponent (remove colors) |
aiProcess_ImproveCacheLocality |
aiProcess_RemoveRedundantMaterials |
aiProcess_SortByPType |
aiProcess_FindDegenerates |
aiProcess_FindInvalidData |
aiProcess_FindInstances |
aiProcess_ValidateDataStructure |
aiProcess_OptimizeMeshes |
aiProcess_OptimizeGraph;
bool AssetManager::loadMeshAssimp(Mesh* mesh, const std::string& path) {
    std::cout << "-------------------- Loading Model " << path.c_str() << " With Assimp --------------------\n";

    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(path, ai_import_flags | aiProcess_Debone);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        std::cerr << "Error loading mesh: " << importer.GetErrorString() << "\n";
        return false;
    }

    std::vector<aiMesh*> ai_meshes;
    std::vector<aiMatrix4x4> ai_transforms;
    collapseAssimpMeshScene(scene->mRootNode, scene, ai_meshes, ai_transforms, aiMatrix4x4());
    if (!loadMeshAssimpScene(mesh, path, scene, ai_meshes, ai_transforms)) {
        return false;
    }
    createMeshVao(mesh);

    // The "scene" pointer will be deleted automatically by "importer"
    return true;
}

// @note that this function could cause you to "lose" a mesh if the path is 
// the same as one already in manager

Mesh* AssetManager::createMesh(const std::string &handle) {
    auto mesh = &handle_mesh_map.try_emplace(handle).first->second;
    mesh->handle = std::move(handle);
    return mesh;
}

//
// ------------------------------- Animated Mesh -------------------------------
//

AnimatedMesh::~AnimatedMesh() {
    // @note pretty sure mesh destructor will be called
    for (const auto& mm : name_animation_map) {
        auto& animation = mm.second;
        if (animation.bone_keyframes == nullptr)
            continue;

        for (uint64_t i = 0; i < animation.num_bone_keyframes; ++i) {
            auto& keyframe = animation.bone_keyframes[i];
            free(keyframe.position_keys);
            free(keyframe.rotation_keys);
            free(keyframe.scale_keys);
        }
        free(animation.bone_keyframes);
    }
}

static void setAnimatedMeshBoneData(Mesh *mesh, uint64_t vertex_index, int bone_id, float weight) {
    for (int i = 0; i < MAX_BONE_WEIGHTS; ++i) {
        if (mesh->bone_ids[vertex_index][i] < 0) {
            mesh->bone_ids[vertex_index][i] = bone_id;
            mesh->weights [vertex_index][i] = weight;
            break;
        }
    }
}

static void extractBoneWeightsFromAiMesh(AnimatedMesh *animesh, aiMesh* ai_mesh, const aiScene* scene, std::unordered_map<std::string, uint64_t>& node_name_id_map, uint64_t vertex_offset) {
    for (int bone_i = 0; bone_i < ai_mesh->mNumBones; ++bone_i) {
        if (animesh->num_bones >= MAX_BONES) {
            std::cerr << "Too many bones loaded in extractBoneWeightsFromAiMesh, more than " << MAX_BONES << " exist, returning\n";
            return;
        }

        int bone_id = -1;
        auto& ai_bone = ai_mesh->mBones[bone_i];
        std::string bone_name = ai_bone->mName.C_Str();

        auto lu = node_name_id_map.find(bone_name);
        if (lu == node_name_id_map.end()) {
            bone_id = animesh->num_bones;
            node_name_id_map.emplace(bone_name, bone_id);

            animesh->bone_offsets.emplace_back(aiMat4x4ToGlm(ai_bone->mOffsetMatrix));
            auto &bn = animesh->bone_names.emplace_back();
            std::copy(bn.begin(), bn.end(), bone_name.data());

            animesh->num_bones++;
        } else {
            bone_id = lu->second;
        }
        assert(bone_id != -1);

        for (int weight_i = 0; weight_i < ai_bone->mNumWeights; ++weight_i) {
            auto& ai_weight = ai_bone->mWeights[weight_i];

            uint64_t vertex_id = ai_weight.mVertexId;
            float weight       = ai_weight.mWeight;

            assert(vertex_id >= 0); // @todo
            setAnimatedMeshBoneData(&animesh->mesh, vertex_offset + vertex_id, bone_id, weight);
        }
    }
}

// Maps scene heirachy to bone ids
static void convertAiNodeToBoneNodeTree(std::vector<AnimatedMesh::BoneNode> &node_list, aiNode* ai_node, const std::unordered_map<std::string, uint64_t> &node_name_id_map, int32_t parent_index) {
    if (ai_node == nullptr) {
        std::cerr << "ai_node nullptr, skipping\n";
        return;
    }

    auto node_index = node_list.size();
    auto& node = node_list.emplace_back();

    node.parent_index = parent_index;
    node.local_transform = aiMat4x4ToGlm(ai_node->mTransformation);
    auto lu = node_name_id_map.find(std::string(ai_node->mName.data));
    if (lu != node_name_id_map.end()) {
        node.id = lu->second;
    }
    else {
        std::cerr << "Node in tree isn't in map, name: " << std::string(ai_node->mName.data) << "\n";
        node.id = -1;
    }

    node_list.reserve(node_list.size() + ai_node->mNumChildren);
    for (int i = 0; i < ai_node->mNumChildren; i++) {
        convertAiNodeToBoneNodeTree(node_list, ai_node->mChildren[i], node_name_id_map, node_index);
    }
}

static void createBoneKeyframesFromAi(AnimatedMesh::BoneKeyframes &keyframes, uint64_t id, const aiNodeAnim * channel) {
    keyframes.id = id;
    keyframes.local_transformation = glm::mat4(1.0f);

    keyframes.num_position_keys = channel->mNumPositionKeys;
    keyframes.prev_position_key = 0;
    keyframes.position_keys = reinterpret_cast<decltype(keyframes.position_keys)>(malloc(sizeof(*keyframes.position_keys) * keyframes.num_position_keys));
    for (uint64_t pi = 0; pi < keyframes.num_position_keys; ++pi) {
        auto& data = keyframes.position_keys[pi];

        data.position = aiVec3ToGlm(channel->mPositionKeys[pi].mValue);
        data.time     = channel->mPositionKeys[pi].mTime;
    }

    keyframes.num_rotation_keys = channel->mNumRotationKeys;
    keyframes.prev_rotation_key = 0;
    keyframes.rotation_keys = reinterpret_cast<decltype(keyframes.rotation_keys)>(malloc(sizeof(*keyframes.rotation_keys) * keyframes.num_rotation_keys));
    for (uint64_t ri = 0; ri < keyframes.num_rotation_keys; ++ri) {
        auto& data = keyframes.rotation_keys[ri];

        data.rotation = aiQuatToGlm(channel->mRotationKeys[ri].mValue);
        data.time = channel->mRotationKeys[ri].mTime;
    }

    keyframes.num_scale_keys = channel->mNumScalingKeys;
    keyframes.prev_scale_key = 0;
    keyframes.scale_keys = reinterpret_cast<decltype(keyframes.scale_keys)>(malloc(sizeof(*keyframes.scale_keys) * keyframes.num_scale_keys));
    for (uint64_t pi = 0; pi < keyframes.num_scale_keys; ++pi) {
        auto& data = keyframes.scale_keys[pi];

        data.scale = aiVec3ToGlm(channel->mScalingKeys[pi].mValue);
        data.time = channel->mScalingKeys[pi].mTime;
    }
}

bool AssetManager::loadAnimatedMeshAssimp(AnimatedMesh* animesh, const std::string& path) {
    std::cout << "-------------------- Loading Animated Model " << path.c_str() << " With Assimp --------------------\n";

    Assimp::Importer importer;
    auto& mesh = animesh->mesh;

    const aiScene* scene = importer.ReadFile(path, ai_import_flags | aiProcess_LimitBoneWeights);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "Error loading animated mesh: " << importer.GetErrorString() << "\n";
        return false;
    }

    std::vector<aiMesh*> ai_meshes;
    std::vector<aiMatrix4x4> ai_transforms;
    collapseAssimpMeshScene(scene->mRootNode, scene, ai_meshes, ai_transforms, aiMatrix4x4());
    if (!loadMeshAssimpScene(&mesh, path, scene, ai_meshes, ai_transforms)) {
        return false;
    }

    // 
    // Extract per vertex bone ids and weights
    //
    mesh.bone_ids = reinterpret_cast<decltype(mesh.bone_ids)>(malloc(sizeof(*mesh.bone_ids) * mesh.num_vertices));
    mesh.weights  = reinterpret_cast<decltype(mesh.weights) >(malloc(sizeof(*mesh.weights ) * mesh.num_vertices));
    if(mesh.bone_ids == NULL || mesh.weights == NULL) {
        std::cerr << "Mallocs failed in animated mesh loader, freeing bone weights and ids\n";
        free(mesh.bone_ids);
        free(mesh.weights);
        createMeshVao(&mesh);
        return false;
    }
    mesh.attributes = (MeshAttributes)(mesh.attributes | MESH_ATTRIBUTES_BONES);
    // Fill bone ids with -1 as a flag that this id is unmapped @note type dependant
    for (uint64_t i = 0; i < mesh.num_vertices; ++i) {
        mesh.bone_ids[i] = glm::ivec4(-1);
    }

    // node_name_id_map shares bone ids between animations and meshes
    std::unordered_map<std::string, uint64_t> node_name_id_map;

    uint64_t vertex_offset = 0;
    for (int i = 0; i < animesh->mesh.num_meshes; ++i) {
        auto& ai_mesh = ai_meshes[i];

        extractBoneWeightsFromAiMesh(animesh, ai_mesh, scene, node_name_id_map, vertex_offset);
        vertex_offset += ai_mesh->mNumVertices;
    }
    createMeshVao(&mesh);

    //
    // Proccess each bone and each animation's keyframes
    //
    animesh->global_transform = aiMat4x4ToGlm(scene->mRootNode->mTransformation.Inverse());
    // @hardcoded Fixes fbx orientation
    animesh->global_transform *= glm::mat4(
        -1, 0, 0, 0,
        0, 0, -1, 0,
        0, -1, 0, 0,
        0, 0, 0, 1
    );
    for (int i = 0; i < scene->mNumAnimations; i++) {
        auto& ai_animation = scene->mAnimations[i];
        auto animation_name = std::string(ai_animation->mName.C_Str());
        auto &animation = animesh->name_animation_map.emplace(animation_name, AnimatedMesh::Animation()).first->second;

        animation.name                  = animation_name; // @debug
        animation.duration              = ai_animation->mDuration;
        animation.ticks_per_second      = ai_animation->mTicksPerSecond;

        animation.num_bone_keyframes = ai_animation->mNumChannels;
        animation.bone_keyframes = reinterpret_cast<decltype(animation.bone_keyframes)>(malloc(sizeof(*animation.bone_keyframes) * animation.num_bone_keyframes));

        std::cout << "Loading animation " << std::string(ai_animation->mName.C_Str()) << " with duration " << animation.duration << " and " << animation.num_bone_keyframes << " bone keyframes\n";
        
        // Reading channels(bones engaged in an animation and their keyframes)
        for (int j = 0; j < animation.num_bone_keyframes; j++) {
            if (animesh->num_bones >= MAX_BONES) {
                std::cerr << "Too many bones loaded in loadAnimatedMeshAssimp, more than " << MAX_BONES << " exist, breaking\n";
                animation.num_bone_keyframes = j;
                break;
            }

            auto ai_channel = ai_animation->mChannels[j];
            std::string node_name = ai_channel->mNodeName.data;

            uint64_t id;
            auto lu = node_name_id_map.find(node_name);
            if (lu == node_name_id_map.end()) {
                id = animesh->num_bones;
                node_name_id_map[node_name] = id;

                animesh->bone_offsets.emplace_back(glm::mat4(1.0f)); // @note this might be wrong
                auto &bn = animesh->bone_names.emplace_back();
                std::copy(bn.begin(), bn.end(), node_name.data());
                animesh->num_bones++;
            }
            else {
                id = lu->second;
            }
            animation.bone_id_keyframe_index_map[id] = j;

            createBoneKeyframesFromAi(animation.bone_keyframes[j], id, ai_channel);
        }
    }
    convertAiNodeToBoneNodeTree(animesh->bone_node_list, scene->mRootNode, node_name_id_map, -1);

    // The "scene" pointer will be deleted automatically by "importer"
    return true;
}

constexpr uint16_t ANIMATION_FILE_VERSION = 0U;
constexpr uint16_t ANIMATION_FILE_MAGIC   = 32891U;
bool AssetManager::writeAnimationFile(const AnimatedMesh* animesh, const std::string& path) {
    std::cout << "--------------------Save Mesh " << path << "--------------------\n";

    FILE* f;
    f = fopen(path.c_str(), "wb");

    if (!f) {
        std::cerr << "Error in writing animation file to path " << path << ".\n";
        return false;
    }

    fwrite(&ANIMATION_FILE_MAGIC  , sizeof(ANIMATION_FILE_MAGIC  ), 1, f);
    fwrite(&ANIMATION_FILE_VERSION, sizeof(ANIMATION_FILE_VERSION), 1, f);

    fwrite(&animesh->num_bones, sizeof(animesh->num_bones), 1, f);
    fwrite(&animesh->bone_offsets[0], sizeof(animesh->bone_offsets[0]), animesh->num_bones, f);
    fwrite(&animesh->bone_names  [0], sizeof(animesh->bone_names  [0]), animesh->num_bones, f); // @debug

    fwrite(&animesh->global_transform, sizeof(animesh->global_transform), 1, f);

    uint64_t num_nodes = animesh->bone_node_list.size();
    fwrite(&num_nodes, sizeof(num_nodes), 1, f);
    fwrite(&animesh->bone_node_list[0], sizeof(animesh->bone_node_list[0]), num_nodes, f);

    uint64_t num_animations = animesh->name_animation_map.size();
    fwrite(&num_animations, sizeof(num_animations), 1, f);
    for (const auto& na : animesh->name_animation_map) {
        auto& name = na.first;
        uint16_t name_len = name.size();
        fwrite(&name_len, sizeof(name_len), 1, f);
        fwrite(&name[0], 1, name_len, f);

        auto& animation = na.second;
        fwrite(&animation.duration, sizeof(animation.duration), 1, f);
        fwrite(&animation.ticks_per_second, sizeof(animation.ticks_per_second), 1, f);

        fwrite(&animation.num_bone_keyframes, sizeof(animation.num_bone_keyframes), 1, f);
        for (uint64_t j = 0; j < animation.num_bone_keyframes; ++j) {
            auto& keyframe = animation.bone_keyframes[j];

            fwrite(&keyframe.id, sizeof(keyframe.id), 1, f);
            fwrite(&keyframe.local_transformation, sizeof(keyframe.local_transformation), 1, f);
            fwrite(&keyframe.num_position_keys, sizeof(keyframe.num_position_keys), 1, f);
            fwrite(&keyframe.num_rotation_keys, sizeof(keyframe.num_rotation_keys), 1, f);
            fwrite(&keyframe.num_scale_keys   , sizeof(keyframe.num_scale_keys   ), 1, f);
            fwrite(keyframe.position_keys, sizeof(keyframe.position_keys[0]), keyframe.num_position_keys, f);
            fwrite(keyframe.rotation_keys, sizeof(keyframe.rotation_keys[0]), keyframe.num_rotation_keys, f);
            fwrite(keyframe.scale_keys   , sizeof(keyframe.scale_keys   [0]), keyframe.num_scale_keys   , f);
        }

        uint64_t num_bone_id_indices = animation.bone_id_keyframe_index_map.size();
        fwrite(&num_bone_id_indices, sizeof(num_bone_id_indices), 1, f);
        for (const auto& bk : animation.bone_id_keyframe_index_map) {
            fwrite(&bk.first,  sizeof(bk.first ), 1, f);
            fwrite(&bk.second, sizeof(bk.second), 1, f);
        }
    }

    fclose(f);
    return true;
}

bool AssetManager::loadAnimationFile(AnimatedMesh* animesh, const std::string& path) {
    std::cout << "----------------Loading Animation File " << path << "----------------\n";

    FILE* f;
    f = fopen(path.c_str(), "rb");
    if (!f) {
        std::cerr << "Error in reading mesh file " << path << "\n.";
        return false;
    }

    std::remove_cv_t<decltype(ANIMATION_FILE_MAGIC)> magic;
    fread(&magic, sizeof(magic), 1, f);
    if (magic != ANIMATION_FILE_MAGIC) {
        std::cerr << "Invalid animation file magic, was " << magic << " expected " << ANIMATION_FILE_MAGIC << ".\n";
        return false;
    }

    std::remove_cv_t<decltype(ANIMATION_FILE_VERSION)> version;
    fread(&version, sizeof(version), 1, f);
    if (version != ANIMATION_FILE_VERSION) {
        std::cerr << "Invalid animation file version, was " << version << " expected " << ANIMATION_FILE_VERSION << ".\n";
        return false;
    }

    fread(&animesh->num_bones, sizeof(animesh->num_bones), 1, f);

    animesh->bone_offsets.resize(animesh->num_bones);
    animesh->bone_names.resize(animesh->num_bones);
    fread(&animesh->bone_offsets[0], sizeof(animesh->bone_offsets[0]), animesh->num_bones, f);
    fread(&animesh->bone_names[0], sizeof(animesh->bone_names[0]), animesh->num_bones, f); // @debug

    fread(&animesh->global_transform, sizeof(animesh->global_transform), 1, f);

    uint64_t num_nodes;
    fread(&num_nodes, sizeof(num_nodes), 1, f);

    animesh->bone_node_list.resize(num_nodes);
    fread(&animesh->bone_node_list[0], sizeof(animesh->bone_node_list[0]), animesh->bone_node_list.size(), f);

    uint64_t num_animations;
    fread(&num_animations, sizeof(num_animations), 1, f);
    for (uint64_t i = 0; i < num_animations; ++i) {
        uint16_t name_len;
        fread(&name_len, sizeof(name_len), 1, f);

        std::string animation_name;
        animation_name.resize(name_len);
        fread(&animation_name[0], 1, name_len, f);

        auto& animation = animesh->name_animation_map[animation_name];
        animation.name = animation_name;
        fread(&animation.duration, sizeof(animation.duration), 1, f);
        fread(&animation.ticks_per_second, sizeof(animation.ticks_per_second), 1, f);

        fread(&animation.num_bone_keyframes, sizeof(animation.num_bone_keyframes), 1, f);
        
        animation.bone_keyframes = reinterpret_cast<decltype(animation.bone_keyframes)>(malloc(sizeof(*animation.bone_keyframes) * animation.num_bone_keyframes));
        for (uint64_t j = 0; j < animation.num_bone_keyframes; ++j) {
            auto& keyframe = animation.bone_keyframes[j];

            fread(&keyframe.id, sizeof(keyframe.id), 1, f);
            fread(&keyframe.local_transformation, sizeof(keyframe.local_transformation), 1, f);
            fread(&keyframe.num_position_keys, sizeof(keyframe.num_position_keys), 1, f);
            fread(&keyframe.num_rotation_keys, sizeof(keyframe.num_rotation_keys), 1, f);
            fread(&keyframe.num_scale_keys, sizeof(keyframe.num_scale_keys), 1, f);

            keyframe.position_keys = reinterpret_cast<decltype(keyframe.position_keys)>(malloc(sizeof(*keyframe.position_keys) * keyframe.num_position_keys));
            keyframe.rotation_keys = reinterpret_cast<decltype(keyframe.rotation_keys)>(malloc(sizeof(*keyframe.rotation_keys) * keyframe.num_rotation_keys));
            keyframe.scale_keys    = reinterpret_cast<decltype(keyframe.scale_keys   )>(malloc(sizeof(*keyframe.scale_keys   ) * keyframe.num_scale_keys   ));
            fread(keyframe.position_keys, sizeof(keyframe.position_keys[0]), keyframe.num_position_keys, f);
            fread(keyframe.rotation_keys, sizeof(keyframe.rotation_keys[0]), keyframe.num_rotation_keys, f);
            fread(keyframe.scale_keys, sizeof(keyframe.scale_keys[0]), keyframe.num_scale_keys, f);

            keyframe.prev_position_key = 0;
            keyframe.prev_rotation_key = 0;
            keyframe.prev_scale_key    = 0;
        }

        uint64_t num_bone_id_indices;
        fread(&num_bone_id_indices, sizeof(num_bone_id_indices), 1, f);
        for (uint64_t j = 0; j < num_bone_id_indices; ++j) {
            uint64_t id, index;
            fread(&id, sizeof(id), 1, f);
            fread(&index, sizeof(index), 1, f);

            animation.bone_id_keyframe_index_map[id] = index;
        }
    }

    fclose(f);

    return true;
}

static glm::vec3 interpolateBonesKeyframesPosition(AnimatedMesh::BoneKeyframes& keyframes, float time, bool looping) {
    // @debug
    if (keyframes.num_position_keys == 0) {
        std::cerr << "interpolateBonesKeyframesPosition keyframes.position_keys was empty\n";
        return glm::vec3();
    }

    if (keyframes.num_position_keys == 1)
        return keyframes.position_keys[0].position;
    
    // This is optimized for any time, this may be better if we assume time is 
    // mostly increasing or pass dt (noting time can be set)
    int next_key_i = keyframes.prev_position_key;
    // We are going backwards/unchanged
    if (keyframes.position_keys[keyframes.prev_position_key].time >= time) {
        for (int i = keyframes.prev_position_key - 1; i >= 0; --i) {
            if (keyframes.position_keys[i].time < time) {
                next_key_i = i + 1;
                break;
            }
        }
    }
    else {
        for (int i = keyframes.prev_position_key + 1; i < keyframes.num_position_keys; ++i) {
            if (keyframes.position_keys[i].time > time) {
                next_key_i = i;
                break;
            }
        }
    }

    int prev_key_i;
    if (looping) {
        if (next_key_i >= keyframes.num_position_keys) {
            time -= keyframes.position_keys[keyframes.num_position_keys].time; // Makes sure interpolation is correct
            next_key_i = 0;
            prev_key_i = keyframes.num_position_keys - 1;
        }
        else if (next_key_i == 0) {
            prev_key_i = keyframes.num_position_keys - 1;
        }
        else {
            prev_key_i = next_key_i - 1;
        }
    }
    else {
        next_key_i = glm::clamp(next_key_i, 0, keyframes.num_position_keys - 1);
        prev_key_i = glm::clamp(next_key_i - 1, 0, keyframes.num_position_keys - 1);
    }
    keyframes.prev_position_key = next_key_i;

    auto next_key = keyframes.position_keys[next_key_i];
    auto prev_key = keyframes.position_keys[prev_key_i];
    float lerp_factor = linearstep(prev_key.time, next_key.time, time);
    return glm::mix(prev_key.position, next_key.position, lerp_factor);
}

static glm::quat interpolateBonesKeyframesRotation(AnimatedMesh::BoneKeyframes& keyframes, float time, bool looping) {
    // @debug
    if (keyframes.num_rotation_keys == 0) {
        std::cerr << "interpolateBonesKeyframesRotation keyframes.rotation_keys was empty\n";
        return glm::quat();
    }

    if (keyframes.num_rotation_keys == 1)
        return keyframes.rotation_keys[0].rotation;

    // This is optimized for any time, this may be better if we assume time is 
    // mostly increasing or pass dt (noting time can be set)
    int next_key_i = keyframes.prev_rotation_key;
    // We are going backwards/unchanged
    if (keyframes.rotation_keys[keyframes.prev_rotation_key].time >= time) {
        for (int i = keyframes.prev_rotation_key - 1; i >= 0; --i) {
            if (keyframes.rotation_keys[i].time < time) {
                next_key_i = i + 1;
                break;
            }
        }
    }
    else {
        for (int i = keyframes.prev_rotation_key + 1; i < keyframes.num_rotation_keys; ++i) {
            if (keyframes.rotation_keys[i].time > time) {
                next_key_i = i;
                break;
            }
        }
    }

    int prev_key_i;
    if (looping) {
        if (next_key_i >= keyframes.num_rotation_keys) {
            time -= keyframes.rotation_keys[keyframes.num_rotation_keys].time; // Makes sure interpolation is correct
            next_key_i = 0;
            prev_key_i = keyframes.num_rotation_keys - 1;
        }
        else if (next_key_i == 0) {
            prev_key_i = keyframes.num_rotation_keys - 1;
        }
        else {
            prev_key_i = next_key_i - 1;
        }
    }
    else {
        next_key_i = glm::clamp(next_key_i, 0, keyframes.num_rotation_keys - 1);
        prev_key_i = glm::clamp(next_key_i - 1, 0, keyframes.num_rotation_keys - 1);
    }
    keyframes.prev_rotation_key = next_key_i;

    auto next_key = keyframes.rotation_keys[next_key_i];
    auto prev_key = keyframes.rotation_keys[prev_key_i];
    float lerp_factor = linearstep(prev_key.time, next_key.time, time);
    auto rot = glm::slerp(prev_key.rotation, next_key.rotation, lerp_factor);
    return glm::normalize(rot);
}

static glm::vec3 interpolateBonesKeyframesScale(AnimatedMesh::BoneKeyframes& keyframes, float time, bool looping) {
    // @debug
    if (keyframes.num_scale_keys == 0) {
        std::cerr << "interpolateBonesKeyframesScale keyframes.scale_keys was empty\n";
        return glm::vec3(1.0f);
    }

    if (keyframes.num_scale_keys == 1)
        return keyframes.scale_keys[0].scale;

    // This is optimized for any time, this may be better if we assume time is 
    // mostly increasing or pass dt (noting time can be set)
    int next_key_i = keyframes.prev_scale_key;
    // We are going backwards/unchanged
    if (keyframes.scale_keys[keyframes.prev_scale_key].time >= time) {
        for (int i = keyframes.prev_scale_key - 1; i >= 0; --i) {
            if (keyframes.scale_keys[i].time < time) {
                next_key_i = i + 1;
                break;
            }
        }
    }
    else {
        for (int i = keyframes.prev_scale_key + 1; i < keyframes.num_scale_keys; ++i) {
            if (keyframes.scale_keys[i].time > time) {
                next_key_i = i;
                break;
            }
        }
    }

    int prev_key_i;
    if (looping) {
        if (next_key_i >= keyframes.num_scale_keys) {
            time -= keyframes.scale_keys[keyframes.num_scale_keys].time; // Makes sure interpolation is correct
            next_key_i = 0;
            prev_key_i = keyframes.num_scale_keys - 1;
        } else if (next_key_i == 0) {
            prev_key_i = keyframes.num_scale_keys - 1;
        }
        else {
            prev_key_i = next_key_i - 1;
        }
    }
    else {
        next_key_i = glm::clamp(next_key_i    , 0, keyframes.num_scale_keys - 1);
        prev_key_i = glm::clamp(next_key_i - 1, 0, keyframes.num_scale_keys - 1);
    }
    keyframes.prev_scale_key = next_key_i;

    auto next_key = keyframes.scale_keys[next_key_i];
    auto prev_key = keyframes.scale_keys[prev_key_i];
    float lerp_factor = linearstep(prev_key.time, next_key.time, time);
    return glm::mix(prev_key.scale, next_key.scale, lerp_factor);
}

void tickBonesKeyframe(AnimatedMesh::BoneKeyframes &keyframes, float time, bool looping) {
    glm::vec3 position = interpolateBonesKeyframesPosition(keyframes, time, looping);
    glm::quat rotation = interpolateBonesKeyframesRotation(keyframes, time, looping);
    glm::vec3 scale    = interpolateBonesKeyframesScale   (keyframes, time, looping);
    
    // @note all these transformations could be made into 4x3 since there is not perspective
    keyframes.local_transformation = createModelMatrix(position, rotation, scale);
}

// @note that this function could cause you to "lose" an animated mesh if the path is 
// the same as one already in manager
AnimatedMesh* AssetManager::createAnimatedMesh(const std::string& handle) {
    auto animesh = &handle_animated_mesh_map.try_emplace(handle).first->second;
    animesh->handle = std::move(handle);
    return animesh;
}

//
// --------------------------------- Textures ----------------------------------
//

// @note that these function could cause you to "lose" a texture if the path is 
// the same as one already in manager

Texture* AssetManager::createTexture(const std::string &handle) {
    auto tex = &handle_texture_map.try_emplace(handle).first->second;
    tex->handle = handle;
    return tex;
}

bool AssetManager::loadTextureFromAssimp(Texture *&tex, aiMaterial *mat, const aiScene *scene, aiTextureType texture_type, GLint internal_format){
	// @note assimp specification wants us to combines a stack texture stack with operations per layer
	// this function instead just takes the first available texture
	aiString path;
	if(aiReturn_SUCCESS == mat->GetTexture(texture_type, 0, &path)) {
        auto ai_tex = scene->GetEmbeddedTexture(path.data);
        // If true this is an embedded texture so load from assimp
        if(ai_tex != nullptr){
            std::cerr << "Loading embedded texture" << path.data << "%s.\n";

            tex = createTexture(std::string(path.data, path.length));

	        glGenTextures(1, &tex->id);
            glBindTexture(GL_TEXTURE_2D, tex->id);// Binding of texture name
			
			// We will use linear interpolation for magnification filter
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			// tiling mode
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (ai_tex->achFormatHint[0] & 0x01) ? GL_REPEAT : GL_CLAMP);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (ai_tex->achFormatHint[0] & 0x01) ? GL_REPEAT : GL_CLAMP);
			// Texture specification
			glTexImage2D(GL_TEXTURE_2D, 0, internal_format, ai_tex->mWidth, ai_tex->mHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, ai_tex->pcData);

            return true;
        } else {
	        auto p = "data/textures/" + std::string(path.data, path.length);

            auto tex_id = loadImage(p, internal_format);
            if (tex_id == GL_FALSE) return false;

            tex = createTexture(p);
            tex->id = tex_id;
            return true;
        }
    }
	return false;
}

bool AssetManager::loadTexture(Texture *tex, const std::string &path, GLint internal_format) {
    auto texture_id = loadImage(path, internal_format);
    if(texture_id == GL_FALSE) return false;

    tex->id = texture_id;
    return true;
}

bool AssetManager::loadCubemapTexture(Texture *tex, const std::array<std::string,FACE_NUM_FACES> &paths, GLint internal_format) {
    auto texture_id = loadCubemap(paths, internal_format);
    if(texture_id == GL_FALSE) return false;

    tex->id = texture_id;
    return true;
}

//
// ---------------------------------- Audio ------------------------------------
//

// @note that these function could cause you to "lose" audio if the path is 
// the same as one already in manager

Audio* AssetManager::createAudio(const std::string& handle) {
    auto audio = &handle_audio_map.try_emplace(handle).first->second;
    audio->handle = handle;
    return audio;
}

// 
// -----------------------------------------------------------------------------
//

// These returns nullptr if the asset doesn't exist

Mesh* AssetManager::getMesh(const std::string &path) {
    auto lu = handle_mesh_map.find(path);
    if(lu == handle_mesh_map.end()) return nullptr;
    else                             return &lu->second;
}

Texture* AssetManager::getTexture(const std::string &path) {
    auto lu = handle_texture_map.find(path);
    if(lu == handle_texture_map.end()) return nullptr;
    else                             return &lu->second;
}

Texture* AssetManager::getColorTexture(const glm::vec3 &col, GLint internal_format) {
    auto lu = color_texture_map.find(col);
    if (lu == color_texture_map.end()) {
        auto tex = &color_texture_map.try_emplace(col).first->second;
        tex->id = create1x1TextureFloat(col, internal_format);
        tex->is_color = true;
        tex->color = col;
        return tex;
    } else {
        return &lu->second;
    }

    return nullptr;
}

void AssetManager::clear() {
    handle_mesh_map.clear();
    handle_texture_map.clear();
    handle_audio_map.clear();
    color_texture_map.clear();
}
