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
#include <set>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

#include <indicators.hpp>
#include <xatlas.h>

#include "assimp/material.h"
#include "assimp/types.h"

#include "serialize.hpp"
#include "texture.hpp"
#include "assets.hpp"
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

    default_material->textures[TextureSlot::ALBEDO]     = asset_manager.getColorTexture(glm::vec4(1, 0, 1, 1),      GL_RGB);
    default_material->textures[TextureSlot::NORMAL]     = asset_manager.getColorTexture(glm::vec4(0.5, 0.5, 1, 1),  GL_RGB);
    default_material->textures[TextureSlot::ROUGHNESS]  = asset_manager.getColorTexture(glm::vec4(1),               GL_RED);
    default_material->textures[TextureSlot::METAL]      = asset_manager.getColorTexture(glm::vec4(0),               GL_RED);
    default_material->textures[TextureSlot::SHININESS]  = asset_manager.getColorTexture(glm::vec4(20.0),            GL_R16F);
    default_material->textures[TextureSlot::SPECULAR]   = asset_manager.getColorTexture(glm::vec4(0.5),             GL_R16F);
    default_material->textures[TextureSlot::AO]         = asset_manager.getColorTexture(glm::vec4(0.5),             GL_RED);
    default_material->textures[TextureSlot::GI]         = asset_manager.getColorTexture(glm::vec4(0.5),             GL_RGB);
    default_material->textures[TextureSlot::EMISSIVE]   = asset_manager.getColorTexture(glm::vec4(0),               GL_RGB);

    default_material->uniforms.emplace("albedo_mult", glm::vec3(1));
    default_material->uniforms.emplace("roughness_mult", 1.0f);
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
    os << "Type: " << m.type << "\n";
    if (m.textures.size()) {
        os << "Textures: \n";
        for (const auto& p : m.textures) {
            os << "\t" << (uint64_t)p.first << " --> " << *p.second << "\n";
        }
    }
    if (m.uniforms.size()) {
        os << "Uniforms: \n";
        for (const auto& p : m.uniforms) {
            os << "\t" << p.first << "\n";
        }
    }

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

    delete[] materials;
    free(material_indices);

    free(indices);
    free(vertices);
    free(normals);
    free(tangents);
    free(uvs);
    free(bone_ids);
    free(weights);

    free(transforms);
    free(draw_start);
    free(draw_count);
}

// @note Without this the uniforms default destructor frees twice, at least thats what i think
Material::~Material() {
    textures.clear();
    uniforms.clear();
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
    if (mesh->attributes & MESH_ATTRIBUTES_COLORS) {
        glGenBuffers(1, &mesh->colors_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, mesh->colors_vbo);
        glBufferData(GL_ARRAY_BUFFER, mesh->num_vertices * sizeof(*mesh->colors), &mesh->colors[0], GL_STATIC_DRAW);
        glVertexAttribPointer(7, 4, GL_FLOAT, false, 0, 0);
        glEnableVertexAttribArray(7);
    }

	glGenBuffers(1, &mesh->indices_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->indices_vbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->num_indices * sizeof(*mesh->indices), &mesh->indices[0], GL_STATIC_DRAW);
   
	glBindVertexArray(0); // Unbind the VAO @perf

    mesh->complete = true;
}

bool AssetManager::writeMeshFile(const Mesh* mesh, const std::string& path) {
    std::cout << "--------------------Save Mesh " << path << "--------------------\n";

    FILE* f;
    f = fopen(path.c_str(), "wb");

    if (!f) {
        std::cerr << "Error in writing mesh file to path " << path << ".\n";
        return false;
    }

    writeMesh(*mesh, f);

    fclose(f);
    return true;
}

bool AssetManager::loadMeshFile(Mesh *mesh, const std::string &path){
    std::cout << "----------------Loading Mesh File " << path << "----------------\n";

    FILE *f;
    f=fopen(path.c_str(), "rb");
    if (!f) {
        std::cerr << "Error in reading mesh file " << path << "\n.";
        return false;
    }

    readMesh(*mesh, *this, f);
    createMeshVao(mesh);
    
    fclose(f);
    return true;
}

void collapseAssimpMeshScene(aiNode* node, const aiScene* scene, std::vector<aiMesh*>& meshes, std::vector<aiMatrix4x4> &ai_global_transforms, aiMatrix4x4 ai_nodes_global_transform) {
    // process all the node's meshes (if any)
    for (int i = 0; i < node->mNumMeshes; i++) {
        auto ai_mesh = scene->mMeshes[node->mMeshes[i]];
        if (ai_mesh->mPrimitiveTypes != aiPrimitiveType_TRIANGLE) 
            continue;

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

// May be called from any thread
static bool xatlasCallback(xatlas::ProgressCategory category, int progress, void* userData) {
    static std::mutex progressMutex;
    std::unique_lock<std::mutex> lock(progressMutex);

    auto bar = (indicators::ProgressBar*)userData;
    bar->set_option(indicators::option::PostfixText{ xatlas::StringForEnum(category) });
    bar->set_progress(progress);

    return true;
}

// Based on: https://gamedev.stackexchange.com/questions/68612/how-to-compute-tangent-and-bitangent-vectors
// by Mohammad Ghabboun (concept3D)
// @note probably better to just copy this from assimp
void calculateTangentArray(const uint64_t num_vertices, const glm::vec3* vertices, const glm::vec3* normals,
    const glm::vec2* texcoords, const uint64_t num_indices, const unsigned int* indices, glm::vec3* tangents) {
    // @note relies on all 0 bits representing floating point 0.0
    memset(tangents, 0, sizeof(*tangents) * num_vertices);

    for (uint64_t a = 0; a < num_indices / 3; a++) {
        const auto& i1 = indices[3*a + 0];
        const auto& i2 = indices[3*a + 1];
        const auto& i3 = indices[3*a + 2];

        const auto& v1 = vertices[i1];
        const auto& v2 = vertices[i2];
        const auto& v3 = vertices[i3];

        const auto& w1 = texcoords[i1];
        const auto& w2 = texcoords[i2];
        const auto& w3 = texcoords[i3];

        float x1 = v2.x - v1.x;
        float x2 = v3.x - v1.x;
        float y1 = v2.y - v1.y;
        float y2 = v3.y - v1.y;
        float z1 = v2.z - v1.z;
        float z2 = v3.z - v1.z;

        float s1 = w2.x - w1.x;
        float s2 = w3.x - w1.x;
        float t1 = w2.y - w1.y;
        float t2 = w3.y - w1.y;

        float r = 1.0F / (s1 * t2 - s2 * t1);
        glm::vec3 sdir((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r, (t2 * z1 - t1 * z2) * r);

        tangents[i1] += sdir;
        tangents[i2] += sdir;
        tangents[i3] += sdir;
    }

    for (uint64_t a = 0; a < num_vertices; a++)
    {
        const auto& n = normals[a];
        const auto& t = tangents[a];

        // Gram-Schmidt orthogonalize
        tangents[a] = glm::normalize((t - n * glm::dot(n, t)));
    }
}

bool parameterizeAndPackMesh(Mesh* mesh) {
    // Subdivide and generate UVs with xatlas for lightmapping
    // Create atlas.
    xatlas::Atlas* atlas = xatlas::Create();

    indicators::ProgressBar bar{ indicators::option::BarWidth{50}, indicators::option::Start{"["}, indicators::option::Fill{"="}, indicators::option::Lead{">"}, indicators::option::Remainder{" "}, indicators::option::End{" ]"}, indicators::option::PostfixText{""}, indicators::option::ForegroundColor{indicators::Color::white}, indicators::option::FontStyles{std::vector<indicators::FontStyle>{indicators::FontStyle::bold}} };
    bar.set_option(indicators::option::ShowPercentage{ true });

    // Set progress callback.
    xatlas::SetProgressCallback(atlas, xatlasCallback, &bar);

    // Add meshes to atlas.
    {
        xatlas::MeshDecl meshDecl;
        meshDecl.vertexCount = mesh->num_vertices;
        meshDecl.vertexPositionData = mesh->vertices;
        meshDecl.vertexPositionStride = sizeof(*mesh->vertices);

        if (mesh->attributes & MESH_ATTRIBUTES_NORMALS) {
            meshDecl.vertexNormalData = mesh->normals;
            meshDecl.vertexNormalStride = sizeof(*mesh->normals);
        }
        if (mesh->attributes & MESH_ATTRIBUTES_UVS) {
            meshDecl.vertexUvData = mesh->uvs;
            meshDecl.vertexUvStride = sizeof(*mesh->uvs);
        }

        meshDecl.indexCount = mesh->num_indices;
        meshDecl.indexData = mesh->indices;
        meshDecl.indexFormat = xatlas::IndexFormat::UInt32;

        xatlas::AddMeshError error = xatlas::AddMesh(atlas, meshDecl, 1);
        if (error != xatlas::AddMeshError::Success) {
            xatlas::Destroy(atlas);
            std::cerr << "Error adding mesh to xatlas: " << xatlas::StringForEnum(error) << "\n";
            return false;
        }
    }
    xatlas::ComputeCharts(atlas);
    xatlas::PackCharts(atlas);

    // 
    // Convert xatlas mesh back into our mesh
    //
    auto& xmesh = atlas->meshes[0];
    int num_vertices = xmesh.vertexCount;
    assert(xmesh.indexCount == mesh->num_indices);

    // @note this is very wasteful but this seems to be the only way to work with xatlas
    glm::vec2* uvs = nullptr;
    glm::vec3* vertices = nullptr, * normals = nullptr, * tangents = nullptr;
    glm::vec4* colors = nullptr;

    bool mallocs_failed = false;
    vertices = reinterpret_cast<decltype(vertices)>(malloc(sizeof(*vertices) * num_vertices));
    mallocs_failed |= vertices == NULL;
    uvs = reinterpret_cast<decltype(uvs)>(malloc(sizeof(*uvs) * num_vertices));
    mallocs_failed |= uvs == NULL;

    if (mesh->attributes & MESH_ATTRIBUTES_NORMALS) {
        normals = reinterpret_cast<decltype(normals)>(malloc(sizeof(*normals) * num_vertices));
        mallocs_failed |= normals == NULL;

        // We can't calculate tangents without normals
        tangents = reinterpret_cast<decltype(tangents)>(malloc(sizeof(*tangents) * num_vertices));
        mallocs_failed |= tangents == NULL;
    }
    if (mesh->attributes & MESH_ATTRIBUTES_COLORS) {
        colors = reinterpret_cast<decltype(colors)>(malloc(sizeof(*colors) * num_vertices));
        mallocs_failed |= mesh->colors == NULL;
    }

    if (mallocs_failed) {
        std::cerr << "Mallocs failed in xatlas generation, freeing xatlas buffers\n";
        free(vertices);
        free(normals);
        free(tangents);
        free(uvs);
        free(colors);
        return false;
    }

    const int uv_w = atlas->width, uv_h = atlas->height;

    for (int v = 0; v < xmesh.vertexCount; v++) {
        const xatlas::Vertex& vertex = xmesh.vertexArray[v];

        uvs[v] = glm::vec2(vertex.uv[0] / uv_w, vertex.uv[1] / uv_h);

        vertices[v] = mesh->vertices[vertex.xref];
        if (mesh->attributes & MESH_ATTRIBUTES_NORMALS) {
            normals[v] = mesh->normals[vertex.xref];
        }
        if (mesh->attributes & MESH_ATTRIBUTES_COLORS) {
            colors[v] = mesh->colors[vertex.xref];
        }
    }

    memcpy(mesh->indices, xmesh.indexArray, mesh->num_indices * sizeof(*mesh->indices));

    if (mesh->attributes & MESH_ATTRIBUTES_NORMALS) {
        // Recalculate tangents @todo assimp load flags
        calculateTangentArray(num_vertices, vertices, normals, uvs, mesh->num_indices, mesh->indices, tangents);
        mesh->attributes = (MeshAttributes)(mesh->attributes | MESH_ATTRIBUTES_TANGENTS);
    }

    mesh->attributes = (MeshAttributes)(mesh->attributes | MESH_ATTRIBUTES_UVS);
    mesh->num_vertices = num_vertices;

    // Copy from temporary buffer to actual mesh
    free(mesh->vertices);
    free(mesh->normals);
    free(mesh->tangents);
    free(mesh->uvs);
    free(mesh->colors);
    mesh->vertices = vertices;
    mesh->normals = normals;
    mesh->tangents = tangents;
    mesh->uvs = uvs;
    mesh->colors = colors;

    // Cleanup.
    xatlas::Destroy(atlas);

    return true;
}

bool AssetManager::loadMeshAssimpScene(Mesh *mesh, const std::string &path, const aiScene* scene, 
                                       const std::vector<aiMesh*> &ai_meshes, const std::vector<aiMatrix4x4>& ai_meshes_global_transforms) {

    // Allocate arrays for each material 
    mesh->num_materials = scene->mNumMaterials;
    mesh->materials = new Material[mesh->num_materials];

    for (int i = 0; i < mesh->num_materials; ++i) {
        // Load material from assimp
        auto ai_mat = scene->mMaterials[i];
        auto& mat = mesh->materials[i];

        aiColor4D col(1.f, 0.f, 1.f, 1.0f); // Storage for getting colors from assimp

        // For now assume everything with base color is PBR, use assimp flags in future
        if (loadTextureFromAssimp(mat.textures[TextureSlot::ALBEDO], ai_mat, scene, aiTextureType_BASE_COLOR, GL_FALSE)) {
            mat.type = MaterialType::PBR;
        } else {
            if (ai_mat->Get(AI_MATKEY_BASE_COLOR, col) == AI_SUCCESS) {
                mat.type = MaterialType::PBR;
                mat.textures[TextureSlot::ALBEDO] = getColorTexture(glm::vec4(col.r, col.g, col.b, col.a), GL_RGB);
            }
            else {
                // Try and load Blinn Phong material
                if (loadTextureFromAssimp(mat.textures[TextureSlot::DIFFUSE], ai_mat, scene, aiTextureType_DIFFUSE, GL_FALSE)) {
                    mat.type = MaterialType::BLINN_PHONG;
                }
                else {
                    if (ai_mat->Get(AI_MATKEY_COLOR_DIFFUSE, col) == AI_SUCCESS) {
                        mat.type = MaterialType::BLINN_PHONG;
                        mat.textures[TextureSlot::DIFFUSE] = getColorTexture(glm::vec4(col.r, col.g, col.b, col.a), GL_RGB);
                    }
                }
            }
        }

        if (mat.type & MaterialType::PBR) {
            // Alpha clip if there is an alpha channel in albedo, again could be done with flag
            if (mat.textures[TextureSlot::ALBEDO] && getChannelsForFormat(mat.textures[TextureSlot::ALBEDO]->format) == ImageChannels::RGBA) {
                mat.type = (MaterialType::Type)(mat.type | MaterialType::ALPHA_CLIP);
            }

            mat.textures[TextureSlot::ROUGHNESS] = default_material->textures[TextureSlot::ROUGHNESS]; // Required for material
            if (!loadTextureFromAssimp(mat.textures[TextureSlot::ROUGHNESS], ai_mat, scene, aiTextureType_DIFFUSE_ROUGHNESS, GL_RED)) {
                if (ai_mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, col) == AI_SUCCESS && (col.r + col.g + col.b > 0) && col.a > 0) {
                    mat.textures[TextureSlot::ROUGHNESS] = getColorTexture(glm::vec4(col.r, col.g, col.b, col.a), GL_RED);
                }
            }

            if (!loadTextureFromAssimp(mat.textures[TextureSlot::METAL], ai_mat, scene, aiTextureType_METALNESS, GL_RED)) {
                if (ai_mat->Get(AI_MATKEY_METALLIC_FACTOR, col) == AI_SUCCESS && (col.r + col.g + col.b > 0) && col.a > 0) {
                    mat.textures[TextureSlot::METAL] = getColorTexture(glm::vec4(col.r, col.g, col.b, col.a), GL_RED);
                    mat.type = (MaterialType::Type)(mat.type | MaterialType::METALLIC);
                }
            } else { mat.type = (MaterialType::Type)(mat.type | MaterialType::METALLIC); }
        }
        else if (mat.type & MaterialType::BLINN_PHONG) {
            // Alpha clip if there is an alpha channel in albedo, again could be done with flag
            if (mat.textures[TextureSlot::DIFFUSE] && getChannelsForFormat(mat.textures[TextureSlot::DIFFUSE]->format) == ImageChannels::RGBA) {
                mat.type = (MaterialType::Type)(mat.type | MaterialType::ALPHA_CLIP);
            }

            mat.textures[TextureSlot::SPECULAR] = default_material->textures[TextureSlot::SPECULAR]; // Required for material
            if (!loadTextureFromAssimp(mat.textures[TextureSlot::SPECULAR], ai_mat, scene, aiTextureType_SPECULAR, GL_RED)) {
                if (ai_mat->Get(AI_MATKEY_SPECULAR_FACTOR, col) == AI_SUCCESS && col.r > 0) {
                    mat.textures[TextureSlot::SPECULAR] = getColorTexture(glm::vec4(col.r, col.g, col.b, col.a), GL_RED);
                }
            }

            mat.textures[TextureSlot::SHININESS] = default_material->textures[TextureSlot::SHININESS]; // Required for material
            if (!loadTextureFromAssimp(mat.textures[TextureSlot::SHININESS], ai_mat, scene, aiTextureType_SHININESS, GL_RED)) { // @todo conversion function from linear to exponent
                if (ai_mat->Get(AI_MATKEY_SHININESS_STRENGTH, col) == AI_SUCCESS && col.r > 0) {
                    mat.textures[TextureSlot::SHININESS] = getColorTexture(glm::vec4(col.r, col.g, col.b, col.a), GL_RED);
                }
            }
        } else {
            mat = *default_material; // @note!! this copies the uniform pointers which could be stale so default material must never be destroyed
        }

        // Emission
        // I don't know what the difference between emission_color and emissive for ASSIMP, @note emissive intensity should probably be loaded as well
        if (!loadTextureFromAssimp(mat.textures[TextureSlot::EMISSIVE], ai_mat, scene, aiTextureType_EMISSIVE, GL_FALSE)) {
            if (!loadTextureFromAssimp(mat.textures[TextureSlot::EMISSIVE], ai_mat, scene, aiTextureType_EMISSION_COLOR, GL_FALSE)) {
                if (ai_mat->Get(AI_MATKEY_COLOR_EMISSIVE, col) == AI_SUCCESS && (col.r + col.g + col.b > 0) && col.a > 0) {
                    mat.textures[TextureSlot::EMISSIVE] = getColorTexture(glm::vec4(col.r, col.g, col.b, col.a), GL_RGB);
                    mat.type = (MaterialType::Type)(mat.type | MaterialType::EMISSIVE);
                }
            } else { mat.type = (MaterialType::Type)(mat.type | MaterialType::EMISSIVE); }
        } else { mat.type = (MaterialType::Type)(mat.type | MaterialType::EMISSIVE); }

        // Baked Lightmaps or Ambient Occlusion
        if (loadTextureFromAssimp(mat.textures[TextureSlot::GI], ai_mat, scene, aiTextureType_LIGHTMAP, GL_RGB)) {
            mat.type = (MaterialType::Type)(mat.type | MaterialType::LIGHTMAPPED);
        }
        else {
            if (loadTextureFromAssimp(mat.textures[TextureSlot::AO], ai_mat, scene, aiTextureType_AMBIENT_OCCLUSION, GL_RED)) {
                mat.type = (MaterialType::Type)(mat.type | MaterialType::AO);
            }
        }

        // Normals
        // @note Since mtl files specify normals as bump maps assume all bump maps are really normals
        if (!loadTextureFromAssimp(mat.textures[TextureSlot::NORMAL], ai_mat, scene, aiTextureType_NORMALS, GL_RGB, true)) {
            if (!loadTextureFromAssimp(mat.textures[TextureSlot::NORMAL], ai_mat, scene, aiTextureType_HEIGHT, GL_RGB, true)) {
                mat.textures[TextureSlot::NORMAL] = default_material->textures[TextureSlot::NORMAL];
            }
        }

        // Remove any null pointers from value initializer of std unordered map
        for (auto& it = mat.textures.cbegin(); it != mat.textures.cend();) {
            if (!it->second) {
                mat.textures.erase(it++);    // or "it = m.erase(it)" since C++11
            } else {
                ++it;
            }
        }

        // Add some uniforms for modifying certain material types
        if (mat.type & MaterialType::PBR) {
            mat.uniforms.emplace("albedo_mult", glm::vec3(1));
            mat.uniforms.emplace("roughness_mult", 1.0f);
        }
        if (mat.type & MaterialType::BLINN_PHONG) {
            mat.uniforms.emplace("diffuse_mult", glm::vec3(1));
            mat.uniforms.emplace("specular_mult", 1.0f);
            mat.uniforms.emplace("shininess_mult", 1.0f);
        }
        if (mat.type & MaterialType::EMISSIVE) {
            mat.uniforms.emplace("emissive_mult", glm::vec3(1));
        }
        if (mat.type & MaterialType::LIGHTMAPPED || mat.type & MaterialType::AO) {
            mat.uniforms.emplace("ambient_mult", 1.0f);
        }
        if (mat.type & MaterialType::METALLIC) {
            mat.uniforms.emplace("metal_mult", 1.0f);
        }

        std::cout << "Material " << i << ": \n" << mat << "\n";
    }

    // Allocate arrays for each mesh 
    mesh->num_submeshes = ai_meshes.size();
    std::cout << "Number of meshes is " << mesh->num_submeshes << "\n";
    mesh->draw_start = reinterpret_cast<decltype(mesh->draw_start)>(malloc(sizeof(*mesh->draw_start) * mesh->num_submeshes));
    mesh->draw_count = reinterpret_cast<decltype(mesh->draw_count)>(malloc(sizeof(*mesh->draw_count) * mesh->num_submeshes));
    mesh->transforms = reinterpret_cast<decltype(mesh->transforms)>(malloc(sizeof(*mesh->transforms) * mesh->num_submeshes));

    mesh->material_indices = reinterpret_cast<decltype(mesh->material_indices)>(malloc(sizeof(*mesh->material_indices) * mesh->num_submeshes));

    mesh->attributes = (MeshAttributes)(MESH_ATTRIBUTES_VERTICES | MESH_ATTRIBUTES_NORMALS | MESH_ATTRIBUTES_TANGENTS | MESH_ATTRIBUTES_UVS | MESH_ATTRIBUTES_COLORS);
    mesh->num_indices  = 0;
    mesh->num_vertices = 0;
	for (int i = 0; i < mesh->num_submeshes; ++i) {
		const aiMesh* ai_mesh = ai_meshes[i]; 

		mesh->draw_start[i] = mesh->num_indices;
		mesh->draw_count[i] = 3*ai_mesh->mNumFaces;
        mesh->num_indices += 3*ai_mesh->mNumFaces;

        mesh->num_vertices += ai_mesh->mNumVertices;

        mesh->material_indices[i] = ai_mesh->mMaterialIndex;

        mesh->transforms[i] = aiMat4x4ToGlm(ai_meshes_global_transforms[i]);

        // If every mesh has attribute then it is valid
        if (mesh->attributes & MESH_ATTRIBUTES_NORMALS && ai_mesh->mNormals == NULL) {
            mesh->attributes = (MeshAttributes)(mesh->attributes ^ MESH_ATTRIBUTES_NORMALS);
        }
        if (mesh->attributes & MESH_ATTRIBUTES_TANGENTS && ai_mesh->mTangents == NULL) {
            mesh->attributes = (MeshAttributes)(mesh->attributes ^ MESH_ATTRIBUTES_TANGENTS);
        }
        if (mesh->attributes & MESH_ATTRIBUTES_UVS && ai_mesh->mTextureCoords[0] == NULL) {
            mesh->attributes = (MeshAttributes)(mesh->attributes ^ MESH_ATTRIBUTES_UVS);
        }
        if (mesh->attributes & MESH_ATTRIBUTES_COLORS && ai_mesh->mColors[0] == NULL) {
            mesh->attributes = (MeshAttributes)(mesh->attributes ^ MESH_ATTRIBUTES_COLORS);
        }
	}
    std::cout << "Mesh attributes are " << (int)mesh->attributes << "\n";
    std::cout << "Number of vertices is " << mesh->num_vertices << "\n";

    bool mallocs_failed = false;
    mesh->vertices      = reinterpret_cast<decltype(mesh->vertices)>(malloc(sizeof(*mesh->vertices)*mesh->num_vertices));
    mallocs_failed     |= mesh->vertices == NULL;

    if (mesh->attributes & MESH_ATTRIBUTES_NORMALS) {
        mesh->normals   = reinterpret_cast<decltype(mesh->normals )>(malloc(sizeof(*mesh->normals )*mesh->num_vertices));
        mallocs_failed |= mesh->normals == NULL;
    }
    if (mesh->attributes & MESH_ATTRIBUTES_TANGENTS) {
        mesh->tangents  = reinterpret_cast<decltype(mesh->tangents)>(malloc(sizeof(*mesh->tangents)*mesh->num_vertices));
        mallocs_failed |= mesh->normals == NULL;
    }
    if (mesh->attributes & MESH_ATTRIBUTES_UVS) {
        mesh->uvs       = reinterpret_cast<decltype(mesh->uvs     )>(malloc(sizeof(*mesh->uvs     )*mesh->num_vertices));
        mallocs_failed |= mesh->uvs == NULL;
    }
    if (mesh->attributes & MESH_ATTRIBUTES_COLORS) {
        mesh->colors    = reinterpret_cast<decltype(mesh->colors  )>(malloc(sizeof(*mesh->colors  )*mesh->num_vertices));
        mallocs_failed |= mesh->colors == NULL;
    }

    mesh->indices  = reinterpret_cast<decltype(mesh->indices )>(malloc(sizeof(*mesh->indices )*mesh->num_indices));
    mallocs_failed |= mesh->indices == NULL;

    if(mallocs_failed) {
        std::cerr << "Mallocs failed in mesh loader, freeing mesh\n";
        free(mesh->vertices);
        free(mesh->normals);
        free(mesh->tangents);
        free(mesh->uvs);
        free(mesh->colors);
        free(mesh->indices);
        return false;
    }

    int vertices_offset = 0, indices_offset = 0;
    for (int j = 0; j < mesh->num_submeshes; ++j) {
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
        if (mesh->attributes & MESH_ATTRIBUTES_COLORS) {
            for (unsigned int i = 0; i < ai_mesh->mNumVertices; i++) {
                mesh->colors[vertices_offset + i] = glm::fvec4(
                    ai_mesh->mColors[0][i].r,
                    ai_mesh->mColors[0][i].g,
                    ai_mesh->mColors[0][i].b,
                    ai_mesh->mColors[0][i].a
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

    if (mesh->uvs == NULL)
        return parameterizeAndPackMesh(mesh);

    return true;
}

static constexpr auto ai_import_flags = aiProcess_JoinIdenticalVertices |
aiProcess_Triangulate |
aiProcess_GenNormals |
aiProcess_CalcTangentSpace |
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
            free(keyframe.positions);
            free(keyframe.position_keys.times);
            free(keyframe.rotations);
            free(keyframe.rotation_keys.times);
            free(keyframe.scales);
            free(keyframe.scale_keys.times);
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

static void extractBoneWeightsFromAiMesh(AnimatedMesh *animesh, Mesh *mesh, aiMesh* ai_mesh, const aiScene* scene, std::unordered_map<std::string, uint64_t>& node_name_id_map, uint64_t vertex_offset) {
    for (int bone_i = 0; bone_i < ai_mesh->mNumBones; ++bone_i) {
        // @todo create valid state so no crash
        if (animesh->num_bones > MAX_BONES) {
            std::cerr << "Too many bones loaded in extractBoneWeightsFromAiMesh, " << animesh->num_bones + ai_mesh->mNumBones - bone_i << " is more than " << MAX_BONES << " exist, breaking\n";
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
            setAnimatedMeshBoneData(mesh, vertex_offset + vertex_id, bone_id, weight);
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

static void createBoneKeyframesFromAi(AnimatedMesh::BoneKeyframes &keyframe, uint64_t id, const aiNodeAnim * channel) {
    keyframe.id = id;

    keyframe.position_keys.num_keys = channel->mNumPositionKeys;
    keyframe.position_keys.prev_key_i = 0;
    keyframe.position_keys.times = reinterpret_cast<decltype(keyframe.position_keys.times)>(malloc(sizeof(*keyframe.position_keys.times) * keyframe.position_keys.num_keys));
    keyframe.positions = reinterpret_cast<decltype(keyframe.positions)>(malloc(sizeof(*keyframe.positions) * keyframe.position_keys.num_keys));
    assert(keyframe.position_keys.times != NULL);
    assert(keyframe.positions != NULL);
    for (uint64_t i = 0; i < keyframe.position_keys.num_keys; ++i) {
        keyframe.positions[i]           = aiVec3ToGlm(channel->mPositionKeys[i].mValue);
        keyframe.position_keys.times[i] = channel->mPositionKeys[i].mTime;
    }

    keyframe.rotation_keys.num_keys = channel->mNumRotationKeys;
    keyframe.rotation_keys.prev_key_i = 0;
    keyframe.rotation_keys.times = reinterpret_cast<decltype(keyframe.rotation_keys.times)>(malloc(sizeof(*keyframe.rotation_keys.times) * keyframe.rotation_keys.num_keys));
    keyframe.rotations = reinterpret_cast<decltype(keyframe.rotations)>(malloc(sizeof(*keyframe.rotations) * keyframe.rotation_keys.num_keys));
    assert(keyframe.rotation_keys.times != NULL);
    assert(keyframe.rotations != NULL);
    for (uint64_t i = 0; i < keyframe.rotation_keys.num_keys; ++i) {
        keyframe.rotations[i] = aiQuatToGlm(channel->mRotationKeys[i].mValue);
        keyframe.rotation_keys.times[i] = channel->mRotationKeys[i].mTime;
    }

    keyframe.scale_keys.num_keys = channel->mNumScalingKeys;
    keyframe.scale_keys.prev_key_i = 0;
    keyframe.scale_keys.times = reinterpret_cast<decltype(keyframe.scale_keys.times)>(malloc(sizeof(*keyframe.scale_keys.times) * keyframe.scale_keys.num_keys));
    keyframe.scales = reinterpret_cast<decltype(keyframe.scales)>(malloc(sizeof(*keyframe.scales) * keyframe.scale_keys.num_keys));
    assert(keyframe.scale_keys.times != NULL);
    assert(keyframe.scales != NULL);
    for (uint64_t i = 0; i < keyframe.scale_keys.num_keys; ++i) {
        keyframe.scales[i] = aiVec3ToGlm(channel->mScalingKeys[i].mValue);
        keyframe.scale_keys.times[i] = channel->mScalingKeys[i].mTime;
    }
}

// @fix when multi mesh model has different transforms per mesh
bool AssetManager::loadAnimatedMeshAssimp(AnimatedMesh* animesh, Mesh *mesh, const std::string& path) {
    std::cout << "-------------------- Loading Animated Model " << path.c_str() << " With Assimp --------------------\n";

    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(path, ai_import_flags | aiProcess_LimitBoneWeights);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "Error loading animated mesh: " << importer.GetErrorString() << "\n";
        return false;
    }

    std::vector<aiMesh*> ai_meshes;
    std::vector<aiMatrix4x4> ai_transforms;
    collapseAssimpMeshScene(scene->mRootNode, scene, ai_meshes, ai_transforms, aiMatrix4x4());
    if (!loadMeshAssimpScene(mesh, path, scene, ai_meshes, ai_transforms)) {
        return false;
    }

    // 
    // Extract per vertex bone ids and weights
    //
    mesh->bone_ids = reinterpret_cast<decltype(mesh->bone_ids)>(malloc(sizeof(*mesh->bone_ids) * mesh->num_vertices));
    mesh->weights  = reinterpret_cast<decltype(mesh->weights) >(malloc(sizeof(*mesh->weights ) * mesh->num_vertices));
    if(mesh->bone_ids == NULL || mesh->weights == NULL) {
        std::cerr << "Mallocs failed in animated mesh loader, freeing bone weights and ids\n";
        free(mesh->bone_ids);
        free(mesh->weights);
        createMeshVao(mesh);
        return false;
    }
    mesh->attributes = (MeshAttributes)(mesh->attributes | MESH_ATTRIBUTES_BONES);
    // Fill bone ids with -1 as a flag that this id is unmapped @note type dependant
    for (uint64_t i = 0; i < mesh->num_vertices; ++i) {
        mesh->bone_ids[i] = glm::ivec4(-1);
    }

    // node_name_id_map shares bone ids between animations and meshes
    std::unordered_map<std::string, uint64_t> node_name_id_map;

    uint64_t vertex_offset = 0;
    for (int i = 0; i < mesh->num_submeshes; ++i) {
        auto& ai_mesh = ai_meshes[i];

        extractBoneWeightsFromAiMesh(animesh, mesh, ai_mesh, scene, node_name_id_map, vertex_offset);
        vertex_offset += ai_mesh->mNumVertices;
    }
    createMeshVao(mesh);

    //
    // Proccess each bone and each animation's keyframes
    //
    // @hardcoded Fixes fbx orientation
    auto animated_to_static = glm::mat4(
        0, 1, 0, 0,
        0, 0, 1, 0,
        1, 0, 0, 0,
        0, 0, 0, 1
    );

    // Bake global transform into mesh transforms
    //auto mesh_transform = aiMat4x4ToGlm(scene->mRootNode->mTransformation);
    //mesh_transform *= animated_to_static;
    //for (int i = 0; i < mesh->num_submeshes; ++i) {
    //    mesh->transforms[i] = mesh->transforms[i] * mesh_transform;
    //}

    animesh->global_transform = glm::inverse(aiMat4x4ToGlm(scene->mRootNode->mTransformation));
    //std::cout << aiMat4x4ToGlm(scene->mRootNode->mTransformation);
    //animesh->global_transform *= animated_to_static;

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
            if (animesh->num_bones > MAX_BONES) {
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

                animesh->bone_offsets.emplace_back(glm::mat4(1.0f));
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
    animesh->bone_node_animation_list.resize(animesh->bone_node_list.size());

    // The "scene" pointer will be deleted automatically by "importer"
    return true;
}

constexpr uint16_t ANIMATION_FILE_VERSION = 1U;
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

            auto write_keys = [&f](AnimatedMesh::BoneKeyframes::Keys& keys) {
                fwrite(&keys.num_keys, sizeof(keys.num_keys), 1, f);
                fwrite(keys.times, sizeof(*keys.times), keys.num_keys, f);
            };

            write_keys(keyframe.position_keys);
            fwrite(keyframe.positions, sizeof(*keyframe.positions), keyframe.position_keys.num_keys, f);
            write_keys(keyframe.rotation_keys);
            fwrite(keyframe.rotations, sizeof(*keyframe.rotations), keyframe.rotation_keys.num_keys, f);
            write_keys(keyframe.scale_keys);
            fwrite(keyframe.scales, sizeof(*keyframe.scales), keyframe.scale_keys.num_keys, f);
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
    animesh->bone_node_animation_list.resize(num_nodes);

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

            auto read_keys = [&f](AnimatedMesh::BoneKeyframes::Keys& keys) {
                fread(&keys.num_keys, sizeof(keys.num_keys), 1, f);

                keys.times = reinterpret_cast<decltype(keys.times)>(malloc(sizeof(*keys.times) * keys.num_keys));
                fread(keys.times, sizeof(*keys.times), keys.num_keys, f);

                keys.prev_key_i = 0;
            };

            read_keys(keyframe.position_keys);
            keyframe.positions = reinterpret_cast<decltype(keyframe.positions)>(malloc(sizeof(*keyframe.positions) * keyframe.position_keys.num_keys));
            fread(keyframe.positions, sizeof(*keyframe.positions), keyframe.position_keys.num_keys, f);

            read_keys(keyframe.rotation_keys);
            keyframe.rotations = reinterpret_cast<decltype(keyframe.rotations)>(malloc(sizeof(*keyframe.rotations) * keyframe.rotation_keys.num_keys));
            fread(keyframe.rotations, sizeof(*keyframe.rotations), keyframe.rotation_keys.num_keys, f);

            read_keys(keyframe.scale_keys);
            keyframe.scales = reinterpret_cast<decltype(keyframe.scales)>(malloc(sizeof(*keyframe.scales) * keyframe.scale_keys.num_keys));
            fread(keyframe.scales, sizeof(*keyframe.scales), keyframe.scale_keys.num_keys, f);
        }

        uint64_t num_bone_id_indices;
        fread(&num_bone_id_indices, sizeof(num_bone_id_indices), 1, f);
        for (uint64_t j = 0; j < num_bone_id_indices; ++j) {
            uint64_t id, index;
            fread(&id, sizeof(id), 1, f);
            fread(&index, sizeof(index), 1, f);

            animation.bone_id_keyframe_index_map[id] = index;
        }

        // @debug
        std::cout << "Loaded animation " << animation.name << " with duration " << animation.duration << " ticks, " << animation.num_bone_keyframes << " keyframes, and " << animation.ticks_per_second << " ticks/s\n";
    }

    fclose(f);

    return true;
}

// Returns the next key index and modifies time to ensure interpolation is correct
static void interpolateKeys(AnimatedMesh::BoneKeyframes::Keys& keys, uint32_t& prev_key_i, uint32_t& next_key_i, float& time, bool looping) {
    if (keys.num_keys == 1) {
        prev_key_i = 0;
        next_key_i = 0;
        return;
    }

    // This is optimized for any time, this may be better if we assume time is 
    // mostly increasing or pass dt (noting time can be set)
    next_key_i = keys.prev_key_i;
    // We are going backwards/unchanged
    if (keys.times[keys.prev_key_i] >= time) {
        next_key_i = 0;
        for (int i = keys.prev_key_i - 1; i >= 0; --i) {
            if (keys.times[i] < time) {
                next_key_i = i + 1;
                break;
            }
        }
    }
    else {
        next_key_i = keys.num_keys - 1;
        for (int i = keys.prev_key_i + 1; i < keys.num_keys; ++i) {
            if (keys.times[i] > time) {
                next_key_i = i;
                break;
            }
        }
    }

    if (looping) {
        if (next_key_i >= keys.num_keys) {
            time -= keys.times[keys.num_keys]; // Makes sure interpolation is correct
            next_key_i = 0;
            prev_key_i = keys.num_keys - 1;
        }
        else if (next_key_i == 0) {
            prev_key_i = keys.num_keys - 1;
        }
        else {
            prev_key_i = next_key_i - 1;
        }
    }
    else {
        next_key_i = glm::clamp(next_key_i, (uint32_t)0, keys.num_keys - (uint32_t)1);
        prev_key_i = glm::clamp(next_key_i - (uint32_t)1, (uint32_t)0, keys.num_keys - (uint32_t)1);
    }
    keys.prev_key_i = next_key_i;
}

static glm::vec3 interpolateBonesKeyframesPosition(AnimatedMesh::BoneKeyframes& keyframes, float time, bool looping) {
    // @debug
    if (keyframes.position_keys.num_keys == 0) {
        std::cerr << "interpolateBonesKeyframesPosition keyframes.positions was empty, returning default\n";
        return glm::vec3();
    }

    uint32_t next_key_i, prev_key_i;
    interpolateKeys(keyframes.position_keys, prev_key_i, next_key_i, time, looping);
    
    auto& next_time = keyframes.position_keys.times[next_key_i];
    auto& prev_time = keyframes.position_keys.times[prev_key_i];
    auto& next_position = keyframes.positions[next_key_i];
    auto& prev_position = keyframes.positions[prev_key_i];

    if (next_time == prev_time) {
        return next_position;
    }

    float lerp_factor = linearstep(prev_time, next_time, time);
    return glm::mix(prev_position, next_position, lerp_factor);
}

static glm::quat interpolateBonesKeyframesRotation(AnimatedMesh::BoneKeyframes& keyframes, float time, bool looping) {
    // @debug
    if (keyframes.rotation_keys.num_keys == 0) {
        std::cerr << "interpolateBonesKeyframesRotation keyframes.rotations was empty, returning default\n";
        return glm::quat();
    }

    uint32_t next_key_i, prev_key_i;
    interpolateKeys(keyframes.rotation_keys, prev_key_i, next_key_i, time, looping);

    auto& next_time = keyframes.rotation_keys.times[next_key_i];
    auto& prev_time = keyframes.rotation_keys.times[prev_key_i];
    auto& next_rotation = keyframes.rotations[next_key_i];
    auto& prev_rotation = keyframes.rotations[prev_key_i];

    if (next_time == prev_time) {
        return next_rotation;
    }

    float lerp_factor = linearstep(prev_time, next_time, time);
    return glm::slerp(prev_rotation, next_rotation, lerp_factor);
}


static glm::vec3 interpolateBonesKeyframesScale(AnimatedMesh::BoneKeyframes& keyframes, float time, bool looping) {
    // @debug
    if (keyframes.scale_keys.num_keys == 0) {
        std::cerr << "interpolateBonesKeyframesScale keyframes.scales was empty, returning default\n";
        return glm::vec3(1.0f);
    }

    uint32_t next_key_i, prev_key_i;
    interpolateKeys(keyframes.scale_keys, prev_key_i, next_key_i, time, looping);

    auto& next_time = keyframes.scale_keys.times[next_key_i];
    auto& prev_time = keyframes.scale_keys.times[prev_key_i];
    auto& next_scale = keyframes.scales[next_key_i];
    auto& prev_scale = keyframes.scales[prev_key_i];

    if (next_time == prev_time) {
        return next_scale;
    }

    float lerp_factor = linearstep(prev_time, next_time, time);
    return glm::mix(prev_scale, next_scale, lerp_factor);
}

glm::mat4x4 tickBonesKeyframe(AnimatedMesh::BoneKeyframes &keyframes, float time, bool looping) {
    glm::vec3 position = interpolateBonesKeyframesPosition(keyframes, time, looping);
    glm::quat rotation = interpolateBonesKeyframesRotation(keyframes, time, looping);
    glm::vec3 scale    = interpolateBonesKeyframesScale   (keyframes, time, looping);
    
    // @note all these transformations could be made into 4x3 since there is no perspective
    return createModelMatrix(position, rotation, scale);
}

// @note that this function could cause you to "lose" an animated mesh if the path is 
// the same as one already in manager
AnimatedMesh* AssetManager::createAnimatedMesh(const std::string& handle) {
    auto animesh = &handle_animated_mesh_map.try_emplace(handle).first->second;
    animesh->handle = std::move(handle);
    return animesh;
}

float getAnimationDuration(const AnimatedMesh& animesh, const std::string& name) {
    auto lu = animesh.name_animation_map.find(name);
    if (lu == animesh.name_animation_map.end()) {
        std::cerr << "Failed to get animation duration " << name << " because it wasn't loaded\n"; // @debug
        return 0.0;
    }
    else {
        return lu->second.duration / lu->second.ticks_per_second;
    }
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

bool AssetManager::loadTextureFromAssimp(Texture *&tex, aiMaterial *mat, const aiScene *scene, aiTextureType texture_type, GLint format, bool floating){
	// @note assimp specification wants us to combines a stack texture stack with operations per layer
	// this function instead just takes the first available texture
	aiString path;
	if(aiReturn_SUCCESS == mat->GetTexture(texture_type, 0, &path)) {
        auto ai_tex = scene->GetEmbeddedTexture(path.data);

        std::string p(path.data, path.length);
        // @todo relative path and possibly copying to texture dump
        if (ai_tex == nullptr)
            p = "data/textures/" + p;

        auto lu = getTexture(p);
        if (lu != nullptr) {
            // Check if loaded texture contains enough channels
            if (getChannelsForFormat(lu->format) >= getChannelsForFormat(format)) {
                tex = lu;
                return true;
            }
        }

        // If true this is an embedded texture so load from assimp
        if(ai_tex != nullptr){
            std::cerr << "Loading embedded texture" << path.data << "%s.\n";

            tex = createTexture(p);

	        glGenTextures(1, &tex->id);
            glBindTexture(GL_TEXTURE_2D, tex->id);// Binding of texture name
			
			// We will use linear interpolation for magnification filter
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			// tiling mode
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (ai_tex->achFormatHint[0] & 0x01) ? GL_REPEAT : GL_CLAMP);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (ai_tex->achFormatHint[0] & 0x01) ? GL_REPEAT : GL_CLAMP);
			// Texture specification
			glTexImage2D(GL_TEXTURE_2D, 0, format, ai_tex->mWidth, ai_tex->mHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, ai_tex->pcData);

            return true;
        } else {
            glm::ivec2 resolution;
            GLenum result_format = format;
            auto tex_id = loadImage(p, resolution, result_format, GL_REPEAT, floating);
            if (tex_id == GL_FALSE) return false;

            tex = createTexture(p);
            tex->format = result_format;
            tex->resolution = resolution;
            tex->id = tex_id;
            return true;
        }
    }
	return false;
}

bool AssetManager::loadTexture(Texture *tex, const std::string &path, GLenum format, const GLint wrap, bool floating, bool trilinear) {
    auto texture_id = loadImage(path, tex->resolution, format, wrap, floating, trilinear);
    tex->id = texture_id;
    tex->format = format;
    tex->complete = true;

    if(texture_id == GL_FALSE) return false;
    return true;
}

bool AssetManager::loadCubemapTexture(Texture *tex, const std::array<std::string,FACE_NUM_FACES> &paths, GLenum format, const GLint wrap, bool floating, bool trilinear) {
    auto texture_id = loadCubemap(paths, tex->resolution, format, wrap, floating, trilinear);
    if(texture_id == GL_FALSE) return false;

    tex->id = texture_id;
    tex->format = format;
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

Mesh* AssetManager::getMesh(const std::string &path) const {
    const auto& lu = handle_mesh_map.find(path);
    if(lu == handle_mesh_map.end()) return nullptr;
    else                             return (Mesh*)&lu->second;
}

AnimatedMesh* AssetManager::getAnimatedMesh(const std::string& path) const {
    const auto& lu = handle_animated_mesh_map.find(path);
    if (lu == handle_animated_mesh_map.end()) return nullptr;
    else                                      return (AnimatedMesh*)&lu->second;
}

Texture* AssetManager::getTexture(const std::string &path) const {
    const auto& lu = handle_texture_map.find(path);
    if(lu == handle_texture_map.end()) return nullptr;
    else                               return (Texture*)&lu->second;
}

Texture* AssetManager::getColorTexture(const glm::vec4& col, GLint format) {
    const auto& lu = color_texture_map.find(col);
    if (lu == color_texture_map.end()) {
        auto tex = &color_texture_map.try_emplace(col).first->second;
        tex->id = create1x1TextureFloat(col, format);
        tex->is_color = true;
        tex->color = col;
        tex->format = format;
        tex->complete = true;
        return tex;
    } else {
        return &lu->second;
    }

    return nullptr;
}

// Pretty crappy function that doesn't actaully clear unused, to improve
// may have to make assets only accessible by handle, check performance
void AssetManager::clearExcluding(const std::set<std::string>& excluded) {
    for (auto it = handle_mesh_map.cbegin(); it != handle_mesh_map.cend(); ) {
        if (excluded.find(it->first) == excluded.end()) {
            handle_mesh_map.erase(it++);    // or "it = m.erase(it)" since C++11
        }
        else {
            ++it;
        }
    }
    for (auto it = handle_animated_mesh_map.cbegin(); it != handle_animated_mesh_map.cend(); ) {
        if (excluded.find(it->first) == excluded.end()) {
            handle_animated_mesh_map.erase(it++);    // or "it = m.erase(it)" since C++11
        }
        else {
            ++it;
        }
    }

    // Can't clear texture or color since mesh relies on it and excluded doesn't
    // properly store
    
    for (auto it = handle_audio_map.cbegin(); it != handle_audio_map.cend(); ) {
        if (excluded.find(it->first) == excluded.end()) {
            handle_audio_map.erase(it++);    // or "it = m.erase(it)" since C++11
        }
        else {
            ++it;
        }
    }
}

void AssetManager::clear() {
    handle_mesh_map.clear();
    handle_texture_map.clear();
    handle_audio_map.clear();
    color_texture_map.clear();
}
