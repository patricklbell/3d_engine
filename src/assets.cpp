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

const auto ai_import_flags = aiProcess_JoinIdenticalVertices |
    aiProcess_Triangulate |
    aiProcess_GenNormals |
    aiProcess_CalcTangentSpace |
    //aiProcess_RemoveComponent (remove colors) |
    aiProcess_LimitBoneWeights |
    aiProcess_ImproveCacheLocality |
    aiProcess_RemoveRedundantMaterials |
    aiProcess_GenUVCoords |
    aiProcess_SortByPType |
    aiProcess_FindDegenerates |
    aiProcess_FindInvalidData |
    aiProcess_FindInstances |
    aiProcess_ValidateDataStructure |
    aiProcess_OptimizeMeshes |
    aiProcess_OptimizeGraph |
    aiProcess_Debone;

Material *default_material;
void initDefaultMaterial(){
    default_material = new Material;
   
    unsigned char albedo[] = {255,0,255};
    default_material->t_albedo = new TextureAsset("DEFAULTMATERIAL:albedo");
    default_material->t_albedo->texture_id = create1x1Texture(albedo);

    unsigned char normal[] = {128, 127, 255};
    default_material->t_normal = new TextureAsset("DEFAULTMATERIAL:normal");
    default_material->t_normal->texture_id = create1x1Texture(normal);

    // @speed GL_FALSE may work so texture is set to nothing, either way
    // loading three of the same texture is needless, set them equal if
    // correct values
    unsigned char metallic[] = {0, 0, 0};
    default_material->t_metallic = new TextureAsset("DEFAULTMATERIAL:metallic");
    default_material->t_metallic->texture_id = create1x1Texture(metallic, GL_RGB);

    unsigned char roughness[] = {0, 0, 0};
    default_material->t_roughness = new TextureAsset("DEFAULTMATERIAL:roughness");
    default_material->t_roughness->texture_id = create1x1Texture(roughness, GL_RGB);

    unsigned char ambient[] = {1, 0, 0};
    default_material->t_ambient = new TextureAsset("DEFAULTMATERIAL:ambient");
    default_material->t_ambient->texture_id = create1x1Texture(ambient, GL_SRGB);
}


//bool loadAssimp(
//	std::string path, 
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
//bool loadAssetObj(Mesh * asset, const std::string &objpath, const std::string &mtlpath){
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

MeshAsset::~MeshAsset(){
    glDeleteVertexArrays(1, &mesh.vao);
    for(int i = 0; i<mesh.num_materials; ++i){
        free(mesh.materials[i]);
    }
    free(mesh.indices);
    free(mesh.vertices);
    free(mesh.normals);
    free(mesh.tangents);
    free(mesh.uvs);

    free(mesh.draw_start);
    free(mesh.draw_count);
}

enum class MeshAttributes : char {
    VERTICES = 0,
    NORMALS  = 1,
    TANGENTS = 2,
    UVS      = 4,
};

const unsigned int MESH_FILE_VERSION = 1;
// For now dont worry about size of types on different platforms
bool writeMeshFile(const Mesh &mesh, std::string path){
    printf("--------------------Save Mesh %s--------------------\n", path.c_str());

    FILE *f;
    f=fopen(path.c_str(), "wb");

    fwrite(&MESH_FILE_VERSION, sizeof(unsigned int), 1, f);

    fwrite(&mesh.num_indices, sizeof(int), 1, f);
    fwrite(mesh.indices, sizeof(unsigned short), mesh.num_indices, f);

    // @todo For now just assume every mesh has every attribute
    char attributes = (char)MeshAttributes::VERTICES | (char)MeshAttributes::NORMALS | (char)MeshAttributes::TANGENTS | (char)MeshAttributes::UVS;
    fwrite(&attributes, sizeof(char), 1, f);

    fwrite(&mesh.num_vertices, sizeof(int), 1, f);

    fwrite(mesh.vertices, sizeof(glm::fvec3), mesh.num_vertices, f);
    fwrite(mesh.normals,  sizeof(glm::fvec3), mesh.num_vertices, f);
    fwrite(mesh.tangents, sizeof(glm::fvec3), mesh.num_vertices, f);

    fwrite(mesh.uvs,      sizeof(glm::fvec2), mesh.num_vertices, f);

    // Write materials as list of image paths
    fwrite(&mesh.num_materials, sizeof(int), 1, f);
    for(int i = 0; i < mesh.num_materials; i++){
        auto &mat = mesh.materials[i];
        int albedo_len = mat->t_albedo->path.size();
        fwrite(&albedo_len, sizeof(int), 1, f);
        fwrite(mat->t_albedo->path.c_str(), mat->t_albedo->path.size(), 1, f);

        int normal_len = mat->t_normal->path.size();
        fwrite(&normal_len, sizeof(int), 1, f);
        fwrite(mat->t_normal->path.c_str(), mat->t_normal->path.size(), 1, f);

        int ambient_len = mat->t_ambient->path.size();
        fwrite(&ambient_len, sizeof(int), 1, f);
        fwrite(mat->t_ambient->path.c_str(), mat->t_ambient->path.size(), 1, f);

        int metallic_len = mat->t_metallic->path.size();
        fwrite(&metallic_len, sizeof(int), 1, f);
        fwrite(mat->t_metallic->path.c_str(), mat->t_metallic->path.size(), 1, f);

        int roughness_len = mat->t_roughness->path.size();
        fwrite(&roughness_len, sizeof(int), 1, f);
        fwrite(mat->t_roughness->path.c_str(), mat->t_roughness->path.size(), 1, f);
    }

    // Write material indice ranges
    fwrite(mesh.draw_start, sizeof(GLint), mesh.num_materials, f);
    fwrite(mesh.draw_count, sizeof(GLint), mesh.num_materials, f);

    fclose(f);
    return true;
}
void createMeshVao(Mesh &mesh){
	glGenBuffers(1, &mesh.vertices_vbo);
	glGenBuffers(1, &mesh.indices_vbo);
	glGenBuffers(1, &mesh.tangents_vbo);
	glGenBuffers(1, &mesh.normals_vbo);
	glGenBuffers(1, &mesh.uvs_vbo);

	glGenVertexArrays(1, &mesh.vao);
	// bind the vao for writing vbos
	glBindVertexArray(mesh.vao);

	// Load the packed vector data into a VBO
	glBindBuffer(GL_ARRAY_BUFFER, mesh.vertices_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh.num_vertices * sizeof(glm::fvec3), &mesh.vertices[0], GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.normals_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh.num_vertices * sizeof(glm::fvec3), &mesh.normals[0], GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.tangents_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh.num_vertices * sizeof(glm::fvec3), &mesh.tangents[0], GL_STATIC_DRAW);
	glVertexAttribPointer(2, 3, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(2);

	glBindBuffer(GL_ARRAY_BUFFER, mesh.uvs_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh.num_vertices * sizeof(glm::fvec2), &mesh.uvs[0], GL_STATIC_DRAW);
	glVertexAttribPointer(3, 2, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(3);

   	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indices_vbo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.num_indices * sizeof(unsigned short), &mesh.indices[0], GL_STATIC_DRAW);

	glBindVertexArray(0); //Unbind the VAO
}

bool readMeshFile(std::map<std::string, Asset*> &assets, Mesh &mesh, std::string path){
    FILE *f;
    f=fopen(path.c_str(), "rb");
    if (!f) {
        fprintf(stderr, "Error in reading mesh file %s.\n", path.c_str());
        return false;
    }

    printf("----------------Loading Mesh File %s----------------\n", path.c_str());

    unsigned int version;
    fread(&version, sizeof(unsigned int), 1, f);
    if(version!=MESH_FILE_VERSION){
        fprintf(stderr, "Invalid mesh file version %u expected %u", version, MESH_FILE_VERSION);
        return false;
    }

    fread(&mesh.num_indices, sizeof(int), 1, f);
    mesh.indices = (unsigned short *)malloc(sizeof(unsigned short)*mesh.num_indices);
    fread(mesh.indices, sizeof(unsigned short), mesh.num_indices, f);
    printf("Number of indices %d\n", mesh.num_indices);

    char attributes;
    fread(&attributes, sizeof(char), 1, f);
    
    // @todo For now just assume all attributes
    fread(&mesh.num_vertices, sizeof(int), 1, f);
    printf("Number of vertices %d\n", mesh.num_vertices);

    mesh.vertices = (glm::fvec3*)malloc(sizeof(glm::fvec3)*mesh.num_vertices);
    mesh.normals  = (glm::fvec3*)malloc(sizeof(glm::fvec3)*mesh.num_vertices);
    mesh.tangents = (glm::fvec3*)malloc(sizeof(glm::fvec3)*mesh.num_vertices);
    fread(mesh.vertices, sizeof(glm::fvec3), mesh.num_vertices, f);
    fread(mesh.normals,  sizeof(glm::fvec3), mesh.num_vertices, f);
    fread(mesh.tangents, sizeof(glm::fvec3), mesh.num_vertices, f);

    mesh.uvs = (glm::fvec2 *)malloc(sizeof(glm::fvec2)*mesh.num_vertices);
    fread(mesh.uvs, sizeof(glm::fvec2), mesh.num_vertices, f);

    // @note this doesn't support embedded materials for binary formats that assimp loads
    fread(&mesh.num_materials, sizeof(int), 1, f);
    mesh.materials = (Material**)malloc(sizeof(Material*)*mesh.num_materials);
    for(int i = 0; i < mesh.num_materials; i++){
        auto mat = new Material();

        int albedo_len;
        fread(&albedo_len, sizeof(int), 1, f);
        char albedo_path[albedo_len];
        fread(&albedo_path, sizeof(char), albedo_len, f);
        if(!strcmp(albedo_path, "DEFAULTMATERIAL:albedo")) mat->t_albedo = default_material->t_albedo;
        else {
            mat->t_albedo = new TextureAsset(albedo_path);
            mat->t_albedo->texture_id = loadImage(albedo_path, GL_RGBA);
            assets[albedo_path] = mat->t_albedo;
        }

        int normal_len;
        fread(&normal_len, sizeof(int), 1, f);
        char normal_path[normal_len];
        fread(&normal_path, sizeof(char), normal_len, f);
        mat->t_normal = new TextureAsset(normal_path);
        if(!strcmp(normal_path, "DEFAULTMATERIAL:normal")) mat->t_normal = default_material->t_normal;
        else {
            mat->t_normal = new TextureAsset(normal_path);
            mat->t_normal->texture_id = loadImage(normal_path, GL_RGBA);
            assets[normal_path] = mat->t_normal;
        }


        int ambient_len;
        fread(&ambient_len, sizeof(int), 1, f);
        char ambient_path[ambient_len];
        fread(&ambient_path, sizeof(char), ambient_len, f);
        if(!strcmp(ambient_path, "DEFAULTMATERIAL:ambient")) mat->t_ambient = default_material->t_ambient;
        else {
            mat->t_ambient = new TextureAsset(ambient_path);
            mat->t_ambient->texture_id = loadImage(ambient_path, GL_RGBA);
            assets[ambient_path] = mat->t_ambient;
        }


        int metallic_len;
        fread(&metallic_len, sizeof(int), 1, f);
        char metallic_path[metallic_len];
        fread(&metallic_path, sizeof(char), metallic_len, f);
        if(!strcmp(metallic_path, "DEFAULTMATERIAL:metallic")) mat->t_metallic = default_material->t_metallic;
        else {
            mat->t_metallic = new TextureAsset(metallic_path);
            mat->t_metallic->texture_id = loadImage(metallic_path, GL_RGBA);
            assets[metallic_path] = mat->t_metallic;
        }

        int roughness_len;
        fread(&roughness_len, sizeof(int), 1, f);
        char roughness_path[roughness_len];
        fread(&roughness_path, sizeof(char), roughness_len, f);
        if(!strcmp(roughness_path, "DEFAULTMATERIAL:roughness")) mat->t_roughness = default_material->t_roughness;
        else {
            mat->t_roughness = new TextureAsset(roughness_path);
            mat->t_roughness->texture_id = loadImage(roughness_path, GL_RGBA);
            assets[roughness_path] = mat->t_roughness;
        }

        printf("Material %d\nAlbedo: %s, Normal %s, Ambient %s, Metallic %s, Roughness %s\n", i, albedo_path, normal_path, ambient_path, metallic_path, roughness_path);

        mesh.materials[i] = mat;
    }
    // @hardcoded
	mesh.draw_mode = GL_TRIANGLES;
	mesh.draw_type = GL_UNSIGNED_SHORT;

    mesh.draw_start = (GLint*)malloc(sizeof(GLint) * mesh.num_materials);
    mesh.draw_count = (GLint*)malloc(sizeof(GLint) * mesh.num_materials);
    fread(mesh.draw_start, sizeof(GLint), mesh.num_materials, f);
    fread(mesh.draw_count, sizeof(GLint), mesh.num_materials, f);
    
    fclose(f);

    createMeshVao(mesh);
    return true;
}
bool loadMesh(Mesh &mesh, std::string path, std::map<std::string, Asset*> &assets){
    std::string m_path = path;
    m_path.replace(path.size() - 3, 3, "mesh");
    printf("Mesh path %s\n", m_path.c_str());

    //if(std::filesystem::exists(m_path)) {
    //    return readMeshFile(mesh, m_path);
    //}

	printf("--------------------Loading Mesh %s--------------------\n", path.c_str());

	Assimp::Importer importer;

	const aiScene* scene = importer.ReadFile(path, ai_import_flags);
	if( !scene) {
		fprintf(stderr, "%s\n", importer.GetErrorString());
		getchar();
		return false;
	}

	// Allocate arrays for each mesh 
	mesh.num_materials = scene->mNumMeshes;
	mesh.draw_count = (GLint*)malloc(mesh.num_materials * sizeof(GLint));
	mesh.draw_start = (GLint*)malloc(mesh.num_materials * sizeof(GLint));
	mesh.materials  = (Material**)malloc(mesh.num_materials * sizeof(Material*));

	mesh.draw_mode = GL_TRIANGLES;
	mesh.draw_type = GL_UNSIGNED_SHORT;

    int indice_offset = 0;
	for (int i = 0; i < scene->mNumMeshes; ++i) {
		const aiMesh* ai_mesh = scene->mMeshes[i]; 

		mesh.draw_start[i] = indice_offset;
        indice_offset += 3*ai_mesh->mNumFaces;
		mesh.draw_count[i] = 3*ai_mesh->mNumFaces;
        mesh.num_vertices += ai_mesh->mNumVertices;
        mesh.num_indices += 3*ai_mesh->mNumFaces;

        printf("Loading mesh index %d from face %d ---> %d.\n", i, mesh.draw_start[i], mesh.draw_start[i]+mesh.draw_count[i]-1);

        // Load material from assimp
		auto ai_mat = scene->mMaterials[scene->mMeshes[i]->mMaterialIndex];
		mesh.materials[i] = new Material;
		auto mat = mesh.materials[i];

        // @speed move default intialisation outside loop
		mat->t_ambient = loadTextureFromAssimp(assets, ai_mat, scene, aiTextureType_AMBIENT_OCCLUSION, GL_SRGB);
        if(mat->t_ambient == nullptr) mat->t_ambient = loadTextureFromAssimp(assets, ai_mat, scene, aiTextureType_AMBIENT, GL_SRGB);
        //if(mat->t_ambient == 0){
        //    aiColor3D ambient;
        //    if(ai_mat->Get(AI_MATKEY_COLOR_AMBIENT,ambient) == AI_SUCCESS){
        //        unsigned char data[] = {
        //            (unsigned char)floor(ambient.r*255),
        //            (unsigned char)floor(ambient.g*255),
        //            (unsigned char)floor(ambient.b*255),
        //        };
        //        mat->t_ambient = create1x1Texture(data, GL_SRGB);
        //    }
        //}
		if(mat->t_ambient == nullptr) mat->t_ambient = default_material->t_ambient;

		mat->t_albedo = loadTextureFromAssimp(assets, ai_mat, scene, aiTextureType_BASE_COLOR, GL_SRGB);
        // If base color isnt present assume diffuse is really an albedo
		if(mat->t_albedo == nullptr) mat->t_albedo = loadTextureFromAssimp(assets, ai_mat, scene, aiTextureType_DIFFUSE, GL_SRGB);
        //if(mat->t_albedo == 0){
        //    aiColor3D albedo;
        //    if(ai_mat->Get(AI_MATKEY_COLOR_AMBIENT,albedo) == AI_SUCCESS){
        //        unsigned char data[] = {
        //            (unsigned char)floor(albedo.r*255),
        //            (unsigned char)floor(albedo.g*255),
        //            (unsigned char)floor(albedo.b*255),
        //        };
        //        mat->t_albedo = create1x1Texture(data, GL_SRGB);
        //    }
        //}
		if(mat->t_albedo == 0) mat->t_albedo = default_material->t_albedo;

		mat->t_metallic = loadTextureFromAssimp(assets, ai_mat, scene, aiTextureType_METALNESS, GL_RGB);
        //if(mat->t_metallic == 0){
        //    aiColor3D metallic;
		//    if(ai_mat->Get(AI_MATKEY_METALLIC_FACTOR,metallic) == AI_SUCCESS){
        //        unsigned char data[] = {
        //            (unsigned char)floor(metallic.r*255),
        //            (unsigned char)floor(metallic.g*255),
        //            (unsigned char)floor(metallic.b*255),
        //        };
        //        mat->t_metallic = create1x1Texture(data, GL_RGB);
        //    }
        //}
        if(mat->t_metallic == nullptr) mat->t_metallic = loadTextureFromAssimp(assets, ai_mat, scene, aiTextureType_REFLECTION, GL_RGB);
        if(mat->t_metallic == nullptr) mat->t_metallic = loadTextureFromAssimp(assets, ai_mat, scene, aiTextureType_SPECULAR, GL_RGB);
		if(mat->t_metallic == nullptr) mat->t_metallic = default_material->t_metallic;

		mat->t_roughness = loadTextureFromAssimp(assets, ai_mat, scene, aiTextureType_DIFFUSE_ROUGHNESS, GL_RGB);
        //if(mat->t_roughness == 0){
        //    aiColor3D roughness;
		//    if(ai_mat->Get(AI_MATKEY_ROUGHNESS_FACTOR,roughness) == AI_SUCCESS){
        //        unsigned char data[] = {
        //            (unsigned char)floor(roughness.r*255),
        //            (unsigned char)floor(roughness.g*255),
        //            (unsigned char)floor(roughness.b*255),
        //        };
        //        mat->t_roughness = create1x1Texture(data, GL_RGB);
        //    }
        //}
        if(mat->t_roughness == nullptr) mat->t_roughness = loadTextureFromAssimp(assets, ai_mat, scene, aiTextureType_SHININESS, GL_RGB);
		if(mat->t_roughness == nullptr) mat->t_roughness = default_material->t_roughness;

		// @note Since mtl files specify normals as bump maps assume all bump maps are really normal
		mat->t_normal = loadTextureFromAssimp(assets, ai_mat, scene, aiTextureType_NORMALS, GL_RGB);
		if(mat->t_normal == nullptr) mat->t_normal = loadTextureFromAssimp(assets, ai_mat, scene, aiTextureType_HEIGHT, GL_RGB);
		if(mat->t_normal == nullptr) mat->t_normal = default_material->t_normal;
	}
    mesh.vertices = (glm::fvec3*)malloc(sizeof(glm::fvec3)*mesh.num_vertices);
    mesh.tangents = (glm::fvec3*)malloc(sizeof(glm::fvec3)*mesh.num_vertices);
    mesh.normals  = (glm::fvec3*)malloc(sizeof(glm::fvec3)*mesh.num_vertices);
    mesh.uvs      = (glm::fvec2*)malloc(sizeof(glm::fvec2)*mesh.num_vertices);
    mesh.indices  = (unsigned short*)malloc(sizeof(unsigned short)*mesh.num_indices);
    int vertices_offset = 0, indices_offset = 0;
    for (int j = 0; j < scene->mNumMeshes; ++j) {
		const aiMesh* ai_mesh = scene->mMeshes[j]; 
		for(unsigned int i=0; i<ai_mesh->mNumVertices; i++){
            mesh.vertices[vertices_offset + i] = glm::fvec3(
                ai_mesh->mVertices[i].x,
                ai_mesh->mVertices[i].y,
                ai_mesh->mVertices[i].z
            );
		}
        if(ai_mesh->mNormals != NULL){
            for(unsigned int i=0; i<ai_mesh->mNumVertices; i++){
                mesh.normals[vertices_offset + i] = glm::fvec3(
                    ai_mesh->mNormals[i].x,
                    ai_mesh->mNormals[i].y,
                    ai_mesh->mNormals[i].z
                );
            }
		}
        if(ai_mesh->mTangents != NULL){
            for(unsigned int i=0; i<ai_mesh->mNumVertices; i++){
                mesh.tangents[vertices_offset + i] = glm::fvec3(
                    ai_mesh->mTangents[i].x,
                    ai_mesh->mTangents[i].y,
                    ai_mesh->mTangents[i].z
                );
            }
		}
		if(ai_mesh->mTextureCoords[0] != NULL){
            for(unsigned int i=0; i<ai_mesh->mNumVertices; i++){
                mesh.uvs[vertices_offset + i] = glm::fvec2(
                    ai_mesh->mTextureCoords[0][i].x,
                    ai_mesh->mTextureCoords[0][i].y
                );
		    }
		}
		
		for (unsigned int i=0; i<ai_mesh->mNumFaces; i++){
			// Assumes the model has only triangles.
			mesh.indices[indices_offset + 3*i] = ai_mesh->mFaces[i].mIndices[0];
			mesh.indices[indices_offset + 3*i + 1] = ai_mesh->mFaces[i].mIndices[1];
			mesh.indices[indices_offset + 3*i + 2] = ai_mesh->mFaces[i].mIndices[2];
		}
        vertices_offset += ai_mesh->mNumVertices;
        indices_offset += ai_mesh->mNumFaces*3;
    }
    createMeshVao(mesh);	

    writeMeshFile(mesh, m_path);

	// The "scene" pointer will be deleted automatically by "importer"
	return true;
}

TextureAsset *loadTextureFromAssimp(std::map<std::string, Asset*> &assets, aiMaterial *mat, const aiScene *scene, aiTextureType texture_type, GLint internal_format=GL_SRGB){
	// @note assimp specification wants us to combines a stack texture stack with operations per layer
	aiString path;
	if(aiReturn_SUCCESS == mat->GetTexture(texture_type, 0, &path))
	{
        auto p = std::string((char *)path.data);
        if(assets.find(p) != assets.end()){
            return (TextureAsset*)assets[p];
        }

        auto a = new TextureAsset(p);
        assets[p] = a;
        auto tex = scene->GetEmbeddedTexture(path.C_Str());
        // If true this is an embedded texture so load from assimp
        if(tex != nullptr){
            printf("Loading embedded texture %s.\n", path.C_Str());
            
            glBindTexture(GL_TEXTURE_2D, a->texture_id);// Binding of texture name
			// We will use linear interpolation for magnification filter
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			// tiling mode
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (tex->achFormatHint[0] & 0x01) ? GL_REPEAT : GL_CLAMP);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (tex->achFormatHint[0] & 0x01) ? GL_REPEAT : GL_CLAMP);
			// Texture specification
			glTexImage2D(GL_TEXTURE_2D, 0, internal_format, tex->mWidth, tex->mHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, tex->pcData);
            return a;
        } else {
            a->path = p;
            a->texture_id = loadImage(p, internal_format);
            return a;
        }
	}
	return nullptr;
}

TextureAsset *createTextureAsset(std::map<std::string, Asset*> &assets, std::string path, GLint internal_format) {
    auto a = new TextureAsset(path);
    a->path = path;
    a->texture_id = loadImage(path, internal_format);
    return a;
}

#include <stb_image.h>

CubemapAsset *createCubemapAsset(std::map<std::string, Asset*> &assets, std::array<std::string,FACE_NUM_FACES> paths, GLint internal_format) {
    // @fix assumes path[0] is unique, maybe just add together all the paths?
    auto a = new CubemapAsset(paths[0]);

    glGenTextures(1, &a->texture_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, a->texture_id);

    int width, height, nrChannels;
    for (int i = 0; i < static_cast<int>(FACE_NUM_FACES); ++i) {
        unsigned char *data = stbi_load(paths[i].c_str(), &width, &height, &nrChannels, 0);
        if (data) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 
                         0, GL_RGB, width, height, 0, internal_format, GL_UNSIGNED_BYTE, data
            );
        } else {
            printf("Cubemap texture failed to load at path: %s\n", paths[i].c_str());
        }
        stbi_image_free(data);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return a;
}
