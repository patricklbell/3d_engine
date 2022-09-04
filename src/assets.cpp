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

//bool loadAssimp(
//	const std::string &path, 
//	std::vector<std::pair<unsigned int, unsigned int>> & mesh_ranges,
//	std::vector<std::string> & mesh_materials,
//	std::vector<unsigned short> & indices,
//	std::vector<glm::vec3> & vertices,
//	std::vector<glm::vec2> & uvs,
//	std::vector<glm::vec3> & normals,
//	std::vector<glm::vec3> & tangents,
//	std::vector<glm::vec3> & bitangents
//){
//
//	Assimp::Importer importer;
//
//	const aiScene* scene = importer.ReadFile(path, ai_import_flags);
//	if( !scene) {
//		fprintf( stderr, importer.GetErrorString());
//		return false;
//	}
//	for (int i = 0; i < scene->mNumMeshes; ++i) {
//		const aiMesh* mesh = scene->mMeshes[i]; 
//
//		// Fill vertices positions
//		vertices.reserve(mesh->mNumVertices);
//		for(unsigned int i=0; i<mesh->mNumVertices; i++){
//			aiVector3D pos = mesh->mVertices[i];
//			vertices.push_back(glm::vec3(pos.x, pos.y, pos.z));
//		}
//
//		// Fill vertices texture coordinates
//		if(mesh->mTextureCoords[0] != NULL){
//			uvs.reserve(mesh->mNumVertices);
//			for(unsigned int i=0; i<mesh->mNumVertices; i++){
//				aiVector3D UVW = mesh->mTextureCoords[0][i]; // Assume only 1 set of UV coords; AssImp supports 8 UV sets.
//				uvs.push_back(glm::vec2(UVW.x, UVW.y));
//			}
//		}
//
//		// Fill vertices normals
//		if(mesh->mNormals != NULL){
//			normals.reserve(mesh->mNumVertices);
//			for(unsigned int i=0; i<mesh->mNumVertices; i++){
//				aiVector3D n = mesh->mNormals[i];
//				normals.push_back(glm::vec3(n.x, n.y, n.z));
//			}
//		}
//
//		// Fill vertices tangents
//		if(mesh->mTangents != NULL){
//			tangents.reserve(mesh->mNumVertices);
//			for(unsigned int i=0; i<mesh->mNumVertices; i++){
//				aiVector3D t = mesh->mTangents[i];
//				tangents.push_back(glm::vec3(t.x, t.y, t.z));
//			}
//		}
//
//		std::pair<unsigned int, unsigned int> mesh_range;
//		mesh_range.first = indices.size();
//		// Fill face indices
//		indices.reserve(3*mesh->mNumFaces);
//		for (unsigned int i=0; i<mesh->mNumFaces; i++){
//			// Assume the model has only triangles.
//			indices.push_back(mesh->mFaces[i].mIndices[0]);
//			indices.push_back(mesh->mFaces[i].mIndices[1]);
//			indices.push_back(mesh->mFaces[i].mIndices[2]);
//		}
//		mesh_range.second = indices.size();
//		mesh_ranges.push_back(mesh_range);
//		
//		aiString name;
//		scene->mMaterials[mesh->mMaterialIndex]->Get(AI_MATKEY_NAME, name);
//		mesh_materials.push_back(std::string(name.C_Str()));
//	}
//	
//	// The "scene" pointer will be deleted automatically by "importer"
//	return true;
//}
//
//// MTL loader which assumes one material
//// TODO: illum model
//bool loadMtl(std::unordered_map<std::string, Material *> &material_map, const std::string &path){
//	// Extract containing directory
//	std::string directory = "";
//	{
//		auto pos = path.rfind("/");
//		if(std::string::npos != pos)
//			directory = path.substr(0, pos) + "/";
//	}
//	std::ifstream file(path, std::ifstream::in);
//	if(!file.is_open()) return false;
//
//	std::string line;
//	Material * working_material = nullptr;
//	while( std::getline(file, line) ){
//		std::istringstream ss(line);
//		std::string head;
//		ss >> head;
//		if(head == "newmtl"){
//			std::string name;
//			ss >> name;
//			material_map[name] = new Material;
//			working_material = material_map[name];
//			working_material->name = name;
//		} else if(working_material != nullptr) {
//			if (head == "Kd") {
//				ss >> working_material->ambient[0];
//				if(!(ss >> working_material->ambient[1])){
//					working_material->ambient[1] = working_material->ambient[0];
//					working_material->ambient[2] = working_material->ambient[0];
//				}
//			} else if (head == "Ka") {
//				ss >> working_material->diffuse[0];
//				if(!(ss >> working_material->diffuse[1])){
//					working_material->diffuse[1] = working_material->diffuse[0];
//					working_material->diffuse[2] = working_material->diffuse[0];
//				}
//
//			} else if (head == "Ks") {
//				ss >> working_material->specular[0];
//				if(!(ss >> working_material->specular[1])){
//					working_material->specular[1] = working_material->specular[0];
//					working_material->specular[2] = working_material->specular[0];
//				}
//			} else if (head == "Tf") {
//				ss >> working_material->transmission_filter[0];
//				if(!(ss >> working_material->transmission_filter[1])){
//					working_material->transmission_filter[1] = working_material->transmission_filter[0];
//					working_material->transmission_filter[2] = working_material->transmission_filter[0];
//				}
//			} else if (head == "d") {
//				ss >> working_material->dissolve;
//			} else if (head == "Ns") {
//				ss >> working_material->specular_exp;
//			} else if (head == "sharpness") {
//				ss >> working_material->reflect_sharp;
//			} else if (head == "Ni") {
//				ss >> working_material->optic_density;
//			} else if (head == "map_Ka") {
//				// TODO: handle arguments
//				auto index = line.find_last_of(' ');
//				std::string tex_path = line.substr(++index);
//				working_material->t_ambient = loadImage(directory+tex_path);
//			} else if (head == "map_Kd") {
//				// TODO: handle arguments
//				auto index = line.find_last_of(' ');
//				std::string tex_path = line.substr(++index);
//				working_material->t_albedo = loadImage(directory+tex_path, true);
//			} else if (head == "norm" || head == "map_Bump") {
//				// TODO: handle arguments
//				auto index = line.find_last_of(' ');
//				std::string tex_path = line.substr(++index);
//				working_material->t_normal = loadImage(directory+tex_path);
//			}
//		}
//	}
//
//	return true;
//}
//
//bool loadAssetObj(Mesh * asset, std::string objpath, std::string mtlpath){
//	std::unordered_map<std::string, Material *> material_map;
//	if(!loadMtl(material_map, mtlpath)){
//		std::cout << "Could not load MTL: " << mtlpath << "\n";
//	}
//
//	// Read our .obj file
//	std::vector<unsigned short> indices;
//	std::vector<std::pair<unsigned int, unsigned int>> mesh_ranges;
//	std::vector<std::string> mesh_materials;
//	std::vector<glm::vec3> indexed_vertices;
//	std::vector<glm::vec2> indexed_uvs;
//	std::vector<glm::vec3> indexed_normals;
//	std::vector<glm::vec3> indexed_tangents;
//	std::vector<glm::vec3> indexed_bitangents;
//	if (!loadAssimp(objpath, mesh_ranges, mesh_materials, indices, indexed_vertices, indexed_uvs, indexed_normals, indexed_tangents, indexed_bitangents)) return false;
//
//	// Fill materials, draw counts and draw starts from ranges
//	asset->num_materials = mesh_materials.size();
//	asset->draw_count = (GLint*)malloc( asset->num_materials * sizeof(GLint) );
//	asset->draw_start = (GLint*)malloc( asset->num_materials * sizeof(GLint) );
//	asset->materials  = (Material**)malloc( asset->num_materials * sizeof(Material*) );
//	GLint i = 0;
//	for (auto &range : mesh_ranges) {
//		asset->draw_count[i] = range.second - range.first;
//		asset->draw_start[i] = range.first;
//		if(material_map.find(mesh_materials[i]) == material_map.end()){
//			asset->materials[i]	 = default_material;
//			std::cout << "Loaded material " << mesh_materials[i] << " from mtl.\n";
//		} else {
//			asset->materials[i]	 = material_map[mesh_materials[i]];
//			std::cout << "Couldn't find material " << mesh_materials[i] << " in mtl.\n";
//		}
//		i++;
//	}
//	asset->draw_mode = GL_TRIANGLES;
//	asset->draw_type = GL_UNSIGNED_SHORT;
//
//
//	glGenBuffers(1, &asset->bitangents);
//	glGenBuffers(1, &asset->tangents);
//	glGenBuffers(1, &asset->normals);
//	glGenBuffers(1, &asset->uvs);
//	glGenBuffers(1, &asset->vertices);
//	glGenBuffers(1, &asset->indices);
//	glGenVertexArrays(1, &asset->vao);
//
//	// bind the VAO
//	glBindVertexArray(asset->vao);
//
//	// Load it into a VBO
//	glBindBuffer(GL_ARRAY_BUFFER, asset->vertices);
//	glBufferData(GL_ARRAY_BUFFER, indexed_vertices.size() * sizeof(glm::vec3), &indexed_vertices[0], GL_STATIC_DRAW);
//	glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, 0);
//	glEnableVertexAttribArray(0);
//
//	glBindBuffer(GL_ARRAY_BUFFER, asset->uvs);
//	glBufferData(GL_ARRAY_BUFFER, indexed_uvs.size() * sizeof(glm::vec2), &indexed_uvs[0], GL_STATIC_DRAW);
//	glVertexAttribPointer(1, 2, GL_FLOAT, false, 0, 0);
//	glEnableVertexAttribArray(1);
//
//	glBindBuffer(GL_ARRAY_BUFFER, asset->normals);
//	glBufferData(GL_ARRAY_BUFFER, indexed_normals.size() * sizeof(glm::vec3), &indexed_normals[0], GL_STATIC_DRAW);
//	glVertexAttribPointer(2, 3, GL_FLOAT, false, 0, 0);
//	glEnableVertexAttribArray(2);
//
//	glBindBuffer(GL_ARRAY_BUFFER, asset->tangents);
//	glBufferData(GL_ARRAY_BUFFER, indexed_tangents.size() * sizeof(glm::vec3), &indexed_tangents[0], GL_STATIC_DRAW);
//	glVertexAttribPointer(3, 3, GL_FLOAT, false, 0, 0);
//	glEnableVertexAttribArray(3);
//
//	glBindBuffer(GL_ARRAY_BUFFER, asset->bitangents);
//	glBufferData(GL_ARRAY_BUFFER, indexed_bitangents.size() * sizeof(glm::vec3), &indexed_bitangents[0], GL_STATIC_DRAW);
//	glVertexAttribPointer(3, 3, GL_FLOAT, false, 0, 0);
//	glEnableVertexAttribArray(3);
//
//	// Generate a buffer for the indices as well
//	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, asset->indices);
//	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);
//
//	glBindVertexArray(0); //Unbind the VAO
//	return true;
//}

//
// ----------------------------------- Mesh ------------------------------------
//
Mesh::~Mesh(){
    glDeleteVertexArrays(1, &vao);
    free(materials);
    free(material_indices);
    free(indices);
    free(vertices);
    free(normals);
    free(tangents);
    free(uvs);

    free(draw_start);
    free(draw_count);
}

enum class MeshAttributes : char {
    VERTICES = 1 << 0,
    NORMALS  = 1 << 1,
    TANGENTS = 1 << 2,
    UVS      = 1 << 3,
    ALL      = VERTICES | NORMALS | TANGENTS | UVS,
};

constexpr uint8_t MESH_FILE_VERSION = 3U;
// For now dont worry about size of types on different platforms
bool AssetManager::writeMeshFile(const Mesh *mesh, const std::string &path){
    std::cout << "--------------------Save Mesh " << path << "--------------------\n";

    FILE *f;
    f=fopen(path.c_str(), "wb");

    if (!f) {
        std::cerr << "Error in writing mesh file to path " << path << ".\n";
        return false;
    }

    fwrite(&MESH_FILE_VERSION, sizeof(MESH_FILE_VERSION), 1, f);

    fwrite(&mesh->num_indices, sizeof(mesh->num_indices), 1, f);
    fwrite(mesh->indices, sizeof(*mesh->indices), mesh->num_indices, f);

    // @todo For now just assume every mesh has every attribute
    MeshAttributes attributes = (MeshAttributes)((char)MeshAttributes::VERTICES | (char)MeshAttributes::NORMALS | (char)MeshAttributes::TANGENTS | (char)MeshAttributes::UVS);
    fwrite(&attributes, sizeof(attributes), 1, f);

    fwrite(&mesh->num_vertices, sizeof(mesh->num_vertices), 1, f);

    fwrite(mesh->vertices, sizeof(*mesh->vertices), mesh->num_vertices, f);
    fwrite(mesh->normals,  sizeof(*mesh->normals ), mesh->num_vertices, f);
    fwrite(mesh->tangents, sizeof(*mesh->tangents), mesh->num_vertices, f);

    fwrite(mesh->uvs,      sizeof(*mesh->uvs     ), mesh->num_vertices, f);

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
            fwrite(tex->handle.c_str(), tex->handle.size(), 1, f);
            fwrite(tex->handle.c_str(), tex->handle.size(), 1, f);
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
	glGenBuffers(1, &mesh->vertices_vbo);
	glGenBuffers(1, &mesh->indices_vbo);
	glGenBuffers(1, &mesh->tangents_vbo);
	glGenBuffers(1, &mesh->normals_vbo);
	glGenBuffers(1, &mesh->uvs_vbo);

	glGenVertexArrays(1, &mesh->vao);
	// bind the vao for writing vbos
	glBindVertexArray(mesh->vao);

	// Load the packed vector data into a VBO
	glBindBuffer(GL_ARRAY_BUFFER, mesh->vertices_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh->num_vertices * sizeof(*mesh->vertices), &mesh->vertices[0], GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, mesh->normals_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh->num_vertices * sizeof(*mesh->normals), &mesh->normals[0], GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, mesh->tangents_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh->num_vertices * sizeof(*mesh->tangents), &mesh->tangents[0], GL_STATIC_DRAW);
	glVertexAttribPointer(2, 3, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(2);

	glBindBuffer(GL_ARRAY_BUFFER, mesh->uvs_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh->num_vertices * sizeof(*mesh->uvs), &mesh->uvs[0], GL_STATIC_DRAW);
	glVertexAttribPointer(3, 2, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(3);

   	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->indices_vbo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->num_indices * sizeof(*mesh->indices), &mesh->indices[0], GL_STATIC_DRAW);

	glBindVertexArray(0); //Unbind the VAO
}

bool AssetManager::loadMeshFile(Mesh *mesh, const std::string &path){
    std::cout << "----------------Loading Mesh File " << path << "----------------\n";

    FILE *f;
    f=fopen(path.c_str(), "rb");
    if (!f) {
        std::cerr << "Error in reading mesh file " << path << "\n.";
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

    MeshAttributes attributes;
    fread(&attributes, sizeof(attributes), 1, f);
    
    // @todo For now just assume all attributes
    fread(&mesh->num_vertices, sizeof(mesh->num_vertices), 1, f);
    std::cout << "Number of vertices " << mesh->num_vertices << ".\n";

    mesh->vertices = reinterpret_cast<decltype(mesh->vertices)>(malloc(sizeof(*mesh->vertices)*mesh->num_vertices));
    mesh->normals  = reinterpret_cast<decltype(mesh->normals )>(malloc(sizeof(*mesh->normals )*mesh->num_vertices));
    mesh->tangents = reinterpret_cast<decltype(mesh->tangents)>(malloc(sizeof(*mesh->tangents)*mesh->num_vertices));
    fread(mesh->vertices, sizeof(*mesh->vertices), mesh->num_vertices, f);
    fread(mesh->normals,  sizeof(*mesh->normals ), mesh->num_vertices, f);
    fread(mesh->tangents, sizeof(*mesh->tangents), mesh->num_vertices, f);

    mesh->uvs = reinterpret_cast<decltype(mesh->uvs)>(malloc(sizeof(*mesh->uvs)*mesh->num_vertices));
    fread(mesh->uvs, sizeof(*mesh->uvs), mesh->num_vertices, f);

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
            // @note this writes the color fields again
            tex = this->getColorTexture(color);
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

        std::cout << "Material " << i << "\nAlbedo: " << mat.t_albedo->handle << ", Normal: " << mat.t_normal->handle 
            << ", Ambient: " << mat.t_ambient->handle << ", Metallic: " << mat.t_metallic->handle << ", Roughness: " << mat.t_roughness->handle << "\n";
    }

#if DO_MULTITHREAD 
    for (auto& tpl : texture_imagedata_default_list) {
        ImageData* img_ptr = std::get<1>(tpl);
        std::string path = (*std::get<0>(tpl))->handle;
        global_thread_pool->queueJob(std::bind(loadImageData, img_ptr, path, GL_SRGB));
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
    mesh->transforms       = reinterpret_cast<decltype(mesh->transforms)      >(malloc(sizeof(*mesh->transforms      ) * mesh->num_meshes));
    fread(mesh->material_indices, sizeof(*mesh->material_indices), mesh->num_meshes, f);
    fread(mesh->draw_start,       sizeof(*mesh->draw_start      ), mesh->num_meshes, f);
    fread(mesh->draw_count,       sizeof(*mesh->draw_count      ), mesh->num_meshes, f);
    fread(mesh->transforms,       sizeof(*mesh->transforms      ), mesh->num_meshes, f);
    
    fclose(f);

    createMeshVao(mesh);
    return true;
}

static constexpr auto ai_import_flags = aiProcess_JoinIdenticalVertices |
    aiProcess_Triangulate |
    aiProcess_GenNormals |
    aiProcess_CalcTangentSpace |
    aiProcess_GenUVCoords |
    //aiProcess_RemoveComponent (remove colors) |
    aiProcess_LimitBoneWeights |
    aiProcess_ImproveCacheLocality |
    aiProcess_RemoveRedundantMaterials |
    aiProcess_SortByPType |
    aiProcess_FindDegenerates |
    aiProcess_FindInvalidData |
    aiProcess_FindInstances |
    aiProcess_ValidateDataStructure |
    aiProcess_OptimizeMeshes |
    aiProcess_OptimizeGraph |
    aiProcess_Debone;
bool AssetManager::loadMeshAssimp(Mesh *mesh, const std::string &path) {
    std::cout << "-------------------- Loading Model " << path.c_str() << " With Assimp --------------------\n";

	Assimp::Importer importer;

	const aiScene* scene = importer.ReadFile(path, ai_import_flags);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        std::cerr << "Error loading mesh: " << importer.GetErrorString() << "\n";
        return false;
    }

    // Allocate arrays for each material 
    mesh->num_materials = scene->mNumMaterials;
    mesh->materials = reinterpret_cast<decltype(mesh->materials)>(malloc(sizeof(*mesh->materials)*mesh->num_materials));

    for (int i = 0; i < mesh->num_materials; ++i) {
        // Load material from assimp
        auto ai_mat = scene->mMaterials[i];
        auto& mat = mesh->materials[i];

        if (!loadTextureFromAssimp(mat.t_ambient, ai_mat, scene, aiTextureType_AMBIENT_OCCLUSION, GL_SRGB)) {
            if (!loadTextureFromAssimp(mat.t_ambient, ai_mat, scene, aiTextureType_AMBIENT, GL_SRGB)) {
                mat.t_ambient = default_material->t_ambient;
            }
        }

        if (!loadTextureFromAssimp(mat.t_albedo, ai_mat, scene, aiTextureType_BASE_COLOR, GL_SRGB)) {
            // If base color isnt present assume diffuse is really an albedo
            if (!loadTextureFromAssimp(mat.t_albedo, ai_mat, scene, aiTextureType_DIFFUSE, GL_SRGB)) {
                aiColor3D col(0.f, 0.f, 0.f);
                if (ai_mat->Get(AI_MATKEY_COLOR_DIFFUSE, col) != AI_SUCCESS) {
                    mat.t_albedo = default_material->t_albedo;
                }
                else {
                    auto color = glm::vec3(col.r, col.g, col.b);
                    mat.t_albedo = getColorTexture(color);
                    mat.t_albedo->is_color = true;
                    mat.t_albedo->color = color;
                }
            }
        }

        if (!loadTextureFromAssimp(mat.t_metallic, ai_mat, scene, aiTextureType_METALNESS, GL_RGB)) {
            if (!loadTextureFromAssimp(mat.t_metallic, ai_mat, scene, aiTextureType_REFLECTION, GL_RGB)) {
                if (!loadTextureFromAssimp(mat.t_metallic, ai_mat, scene, aiTextureType_SPECULAR, GL_RGB)) {
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

        if (!loadTextureFromAssimp(mat.t_roughness, ai_mat, scene, aiTextureType_DIFFUSE_ROUGHNESS, GL_RGB)) {
            if (!loadTextureFromAssimp(mat.t_roughness, ai_mat, scene, aiTextureType_SHININESS, GL_RGB)) {
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
        if (!loadTextureFromAssimp(mat.t_normal, ai_mat, scene, aiTextureType_NORMALS, GL_RGB)) {
            if (!loadTextureFromAssimp(mat.t_normal, ai_mat, scene, aiTextureType_HEIGHT, GL_RGB)) {
                mat.t_normal = default_material->t_normal;
            }
        }
    }

    std::function<void(aiNode* node, const aiScene* scene, std::vector<aiMesh*> &meshes, std::vector<aiMatrix4x4*> &transforms)> recursive_node_meshes;
    recursive_node_meshes = [&recursive_node_meshes](aiNode* node, const aiScene* scene, std::vector<aiMesh*> &meshes, std::vector<aiMatrix4x4*> &transforms)->void {
        // process all the node's meshes (if any)
        for (int i = 0; i < node->mNumMeshes; i++) {
            auto ai_mesh = scene->mMeshes[node->mMeshes[i]];
            std::cout << "Primitive type: " << ai_mesh->mPrimitiveTypes << "\n";
            if (ai_mesh->mPrimitiveTypes != aiPrimitiveType_TRIANGLE) continue;

            meshes.push_back(ai_mesh);
            // @note duplicates transforms, could have node indices etc..
            transforms.push_back(&node->mTransformation);
        }
        // then do the same for each of its children
        for (int i = 0; i < node->mNumChildren; i++) {
            recursive_node_meshes(node->mChildren[i], scene, meshes, transforms);
        }
    };

    std::vector<aiMesh*> ai_meshes;
    std::vector<aiMatrix4x4*> ai_transforms;
    recursive_node_meshes(scene->mRootNode, scene, ai_meshes, ai_transforms);

    // Allocate arrays for each mesh 
    mesh->num_meshes = ai_meshes.size();
    std::cout << "Number of meshes is " << mesh->num_meshes << "\n";
    mesh->draw_start = reinterpret_cast<decltype(mesh->draw_start)>(malloc(sizeof(*mesh->draw_start) * mesh->num_meshes));
    mesh->draw_count = reinterpret_cast<decltype(mesh->draw_count)>(malloc(sizeof(*mesh->draw_count) * mesh->num_meshes));
    mesh->transforms = reinterpret_cast<decltype(mesh->transforms)>(malloc(sizeof(*mesh->transforms) * mesh->num_meshes));

    mesh->material_indices = reinterpret_cast<decltype(mesh->material_indices)>(malloc(sizeof(*mesh->material_indices) * mesh->num_meshes));

	for (int i = 0; i < mesh->num_meshes; ++i) {
		const aiMesh* ai_mesh = ai_meshes[i]; 

		mesh->draw_start[i] = mesh->num_indices;
		mesh->draw_count[i] = 3*ai_mesh->mNumFaces;
        mesh->num_indices += 3*ai_mesh->mNumFaces;

        mesh->num_vertices += ai_mesh->mNumVertices;

        mesh->material_indices[i] = ai_mesh->mMaterialIndex;

        auto& t = *ai_transforms[i];
        mesh->transforms[i] = glm::mat4x4(
            t.a1, t.b1, t.c1, t.d1, 
            t.a2, t.b2, t.c2, t.d2,
            t.a3, t.b3, t.c3, t.d3,
            t.a4, t.b4, t.c4, t.d4
        );
	}

    mesh->vertices = reinterpret_cast<decltype(mesh->vertices)>(malloc(sizeof(*mesh->vertices)*mesh->num_vertices));
    mesh->normals  = reinterpret_cast<decltype(mesh->normals )>(malloc(sizeof(*mesh->normals )*mesh->num_vertices));
    mesh->tangents = reinterpret_cast<decltype(mesh->tangents)>(malloc(sizeof(*mesh->tangents)*mesh->num_vertices));
    mesh->uvs      = reinterpret_cast<decltype(mesh->uvs     )>(malloc(sizeof(*mesh->uvs     )*mesh->num_vertices));

    mesh->indices  = reinterpret_cast<decltype(mesh->indices )>(malloc(sizeof(*mesh->indices )*mesh->num_indices));

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
        if(ai_mesh->mNormals != NULL){
            for(unsigned int i=0; i<ai_mesh->mNumVertices; i++){
                mesh->normals[vertices_offset + i] = glm::fvec3(
                    ai_mesh->mNormals[i].x,
                    ai_mesh->mNormals[i].y,
                    ai_mesh->mNormals[i].z
                );
            }
		}
        if(ai_mesh->mTangents != NULL){
            for(unsigned int i=0; i<ai_mesh->mNumVertices; i++){
                mesh->tangents[vertices_offset + i] = glm::fvec3(
                    ai_mesh->mTangents[i].x,
                    ai_mesh->mTangents[i].y,
                    ai_mesh->mTangents[i].z
                );
            }
		}
		if(ai_mesh->mTextureCoords[0] != NULL){
            for(unsigned int i=0; i<ai_mesh->mNumVertices; i++){
                mesh->uvs[vertices_offset + i] = glm::fvec2(
                    ai_mesh->mTextureCoords[0][i].x,
                    ai_mesh->mTextureCoords[0][i].y
                );
		    }
		}
		
		for (unsigned int i=0; i<ai_mesh->mNumFaces; i++){
			// Assumes the model has only triangles.
			mesh->indices[indices_offset + 3*i    ] = vertices_offset + ai_mesh->mFaces[i].mIndices[0];
			mesh->indices[indices_offset + 3*i + 1] = vertices_offset + ai_mesh->mFaces[i].mIndices[1];
			mesh->indices[indices_offset + 3*i + 2] = vertices_offset + ai_mesh->mFaces[i].mIndices[2];
		}

        vertices_offset += ai_mesh->mNumVertices;
        indices_offset += ai_mesh->mNumFaces*3;
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

bool AssetManager::loadMesh(Mesh *mesh, const std::string &path, const bool is_mesh) {
    if(is_mesh) {
        return loadMeshFile(mesh, path);
    } else {
        return loadMeshAssimp(mesh, path);
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

bool AssetManager::loadTextureFromAssimp(Texture *tex, aiMaterial *mat, const aiScene *scene, aiTextureType texture_type, GLint internal_format){
	// @note assimp specification wants us to combines a stack texture stack with operations per layer
	// this function instead just takes the first available texture
	aiString path;
	if(aiReturn_SUCCESS == mat->GetTexture(texture_type, 0, &path)) {
	    auto p = std::string(path.data, path.length);

        auto ai_tex = scene->GetEmbeddedTexture(path.data);
        // If true this is an embedded texture so load from assimp
        if(ai_tex != nullptr){
            std::cerr << "Loading embedded texture" << path.data << "%s.\n";
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
        unsigned char col256[3];
        col256[0] = col.x*255;
        col256[1] = col.y*255;
        col256[2] = col.z*255;

        auto tex = &color_texture_map.try_emplace(col).first->second;
        tex->id = create1x1Texture(col256, internal_format);
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
