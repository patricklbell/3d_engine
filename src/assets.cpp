#include <vector>
#include <stdio.h>
#include <string>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>

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

Material *default_material;
void initDefaultMaterial(){
    default_material = new Material;

	glGenTextures(1, &default_material->t_diffuse);
	glBindTexture(GL_TEXTURE_2D, default_material->t_diffuse);
	const unsigned char diff_data[] = {255, 0, 200};
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, &diff_data[0]);

	glGenTextures(1, &default_material->t_normal);
	glBindTexture(GL_TEXTURE_2D, default_material->t_normal);
	const unsigned char norm_data[] = {128, 127, 255};
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, &norm_data[0]);

}

bool loadAssimp(
	std::string path, 
	std::vector<std::pair<unsigned int, unsigned int>> & mesh_ranges,
	std::vector<std::string> & mesh_materials,
	std::vector<unsigned short> & indices,
	std::vector<glm::vec3> & vertices,
	std::vector<glm::vec2> & uvs,
	std::vector<glm::vec3> & normals,
	std::vector<glm::vec3> & tangents
){

	Assimp::Importer importer;

	const aiScene* scene = importer.ReadFile(path, aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType | aiProcess_Triangulate);
	if( !scene) {
		fprintf( stderr, importer.GetErrorString());
		return false;
	}
	for (int i = 0; i < scene->mNumMeshes; ++i) {
		const aiMesh* mesh = scene->mMeshes[i]; 

		// Fill vertices positions
		vertices.reserve(mesh->mNumVertices);
		for(unsigned int i=0; i<mesh->mNumVertices; i++){
			aiVector3D pos = mesh->mVertices[i];
			vertices.push_back(glm::vec3(pos.x, pos.y, pos.z));
		}

		// Fill vertices texture coordinates
		if(mesh->mTextureCoords[0] != NULL){
			uvs.reserve(mesh->mNumVertices);
			for(unsigned int i=0; i<mesh->mNumVertices; i++){
				aiVector3D UVW = mesh->mTextureCoords[0][i]; // Assume only 1 set of UV coords; AssImp supports 8 UV sets.
				uvs.push_back(glm::vec2(UVW.x, UVW.y));
			}
		}

		// Fill vertices normals
		if(mesh->mNormals != NULL){
			normals.reserve(mesh->mNumVertices);
			for(unsigned int i=0; i<mesh->mNumVertices; i++){
				aiVector3D n = mesh->mNormals[i];
				normals.push_back(glm::vec3(n.x, n.y, n.z));
			}
		}

		// Fill vertices tangents
		if(mesh->mTangents != NULL){
			tangents.reserve(mesh->mNumVertices);
			for(unsigned int i=0; i<mesh->mNumVertices; i++){
				aiVector3D t = mesh->mTangents[i];
				tangents.push_back(glm::vec3(t.x, t.y, t.z));
			}
		}

		std::pair<unsigned int, unsigned int> mesh_range;
		mesh_range.first = indices.size();
		// Fill face indices
		indices.reserve(3*mesh->mNumFaces);
		for (unsigned int i=0; i<mesh->mNumFaces; i++){
			// Assume the model has only triangles.
			indices.push_back(mesh->mFaces[i].mIndices[0]);
			indices.push_back(mesh->mFaces[i].mIndices[1]);
			indices.push_back(mesh->mFaces[i].mIndices[2]);
		}
		mesh_range.second = indices.size();
		mesh_ranges.push_back(mesh_range);
		
		aiString name;
		scene->mMaterials[mesh->mMaterialIndex]->Get(AI_MATKEY_NAME, name);
		mesh_materials.push_back(std::string(name.C_Str()));
	}
	
	// The "scene" pointer will be deleted automatically by "importer"
	return true;
}

// MTL loader which assumes one material
// TODO: illum model
bool loadMtl(std::unordered_map<std::string, Material *> &material_map, const std::string &path){
	// Extract containing directory
	std::string directory = "";
	{
		auto pos = path.rfind("/");
		if(std::string::npos != pos)
			directory = path.substr(0, pos) + "/";
	}
	std::ifstream file(path, std::ifstream::in);
	if(!file.is_open()) return false;

	std::string line;
	Material * working_material = nullptr;
	while( std::getline(file, line) ){
		std::istringstream ss(line);
		std::string head;
		ss >> head;
		if(head == "newmtl"){
			std::string name;
			ss >> name;
			material_map[name] = new Material;
			working_material = material_map[name];
			working_material->name = name;
		} else if(working_material != nullptr) {
			if (head == "Kd") {
				ss >> working_material->ambient[0];
				if(!(ss >> working_material->ambient[1])){
					working_material->ambient[1] = working_material->ambient[0];
					working_material->ambient[2] = working_material->ambient[0];
				}
			} else if (head == "Ka") {
				ss >> working_material->diffuse[0];
				if(!(ss >> working_material->diffuse[1])){
					working_material->diffuse[1] = working_material->diffuse[0];
					working_material->diffuse[2] = working_material->diffuse[0];
				}

			} else if (head == "Ks") {
				ss >> working_material->specular[0];
				if(!(ss >> working_material->specular[1])){
					working_material->specular[1] = working_material->specular[0];
					working_material->specular[2] = working_material->specular[0];
				}
			} else if (head == "Tf") {
				ss >> working_material->trans_filter[0];
				if(!(ss >> working_material->trans_filter[1])){
					working_material->trans_filter[1] = working_material->trans_filter[0];
					working_material->trans_filter[2] = working_material->trans_filter[0];
				}
			} else if (head == "d") {
				ss >> working_material->dissolve;
			} else if (head == "Ns") {
				ss >> working_material->spec_exp;
			} else if (head == "sharpness") {
				ss >> working_material->reflect_sharp;
			} else if (head == "Ni") {
				ss >> working_material->optic_density;
			} else if (head == "map_Ka") {
				// TODO: handle arguments
				auto index = line.find_last_of(' ');
				std::string tex_path = line.substr(++index);
				working_material->t_ambient = loadImage(directory+tex_path);
			} else if (head == "map_Kd") {
				// TODO: handle arguments
				auto index = line.find_last_of(' ');
				std::string tex_path = line.substr(++index);
				working_material->t_diffuse = loadImage(directory+tex_path, true);
			} else if (head == "norm" || head == "map_Bump") {
				// TODO: handle arguments
				auto index = line.find_last_of(' ');
				std::string tex_path = line.substr(++index);
				working_material->t_normal = loadImage(directory+tex_path);
			}
		}
	}

	return true;
}

bool loadAssetObj(Mesh * asset, const std::string &objpath, const std::string &mtlpath){
	std::unordered_map<std::string, Material *> material_map;
	if(!loadMtl(material_map, mtlpath)){
		std::cout << "Could not load MTL: " << mtlpath << "\n";
	}

	// Read our .obj file
	std::vector<unsigned short> indices;
	std::vector<std::pair<unsigned int, unsigned int>> mesh_ranges;
	std::vector<std::string> mesh_materials;
	std::vector<glm::vec3> indexed_vertices;
	std::vector<glm::vec2> indexed_uvs;
	std::vector<glm::vec3> indexed_normals;
	std::vector<glm::vec3> indexed_tangents;
	if (!loadAssimp(objpath, mesh_ranges, mesh_materials, indices, indexed_vertices, indexed_uvs, indexed_normals, indexed_tangents)) return false;

	// Fill materials, draw counts and draw starts from ranges
	asset->num_materials = mesh_materials.size();
	asset->draw_count = (GLint*)malloc( asset->num_materials * sizeof(GLint) );
	asset->draw_start = (GLint*)malloc( asset->num_materials * sizeof(GLint) );
	asset->materials  = (Material**)malloc( asset->num_materials * sizeof(Material*) );
	GLint i = 0;
	for (auto &range : mesh_ranges) {
		asset->draw_count[i] = range.second - range.first;
		asset->draw_start[i] = range.first;
		if(material_map.find(mesh_materials[i]) == material_map.end()){
			asset->materials[i]	 = default_material;
			std::cout << "Loaded material " << mesh_materials[i] << " from mtl.\n";
		} else {
			asset->materials[i]	 = material_map[mesh_materials[i]];
			std::cout << "Couldn't find material " << mesh_materials[i] << " in mtl.\n";
		}
		i++;
	}
	asset->draw_mode = GL_TRIANGLES;
	asset->draw_type = GL_UNSIGNED_SHORT;


	glGenBuffers(1, &asset->tangents);
	glGenBuffers(1, &asset->normals);
	glGenBuffers(1, &asset->uvs);
	glGenBuffers(1, &asset->vertices);
	glGenBuffers(1, &asset->indices);
	glGenVertexArrays(1, &asset->vao);

	// bind the VAO
	glBindVertexArray(asset->vao);

	// Load it into a VBO
	glBindBuffer(GL_ARRAY_BUFFER, asset->vertices);
	glBufferData(GL_ARRAY_BUFFER, indexed_vertices.size() * sizeof(glm::vec3), &indexed_vertices[0], GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, asset->uvs);
	glBufferData(GL_ARRAY_BUFFER, indexed_uvs.size() * sizeof(glm::vec2), &indexed_uvs[0], GL_STATIC_DRAW);
	glVertexAttribPointer(1, 2, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ARRAY_BUFFER, asset->normals);
	glBufferData(GL_ARRAY_BUFFER, indexed_normals.size() * sizeof(glm::vec3), &indexed_normals[0], GL_STATIC_DRAW);
	glVertexAttribPointer(2, 3, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(2);

	glBindBuffer(GL_ARRAY_BUFFER, asset->tangents);
	glBufferData(GL_ARRAY_BUFFER, indexed_tangents.size() * sizeof(glm::vec3), &indexed_tangents[0], GL_STATIC_DRAW);
	glVertexAttribPointer(3, 3, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(3);

	// Generate a buffer for the indices as well
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, asset->indices);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);

	glBindVertexArray(0); //Unbind the VAO
	return true;
}

bool loadAsset(Mesh * asset, const std::string &path){
	printf("Loading asset %s.\n", path.c_str());

	Assimp::Importer importer;

	const aiScene* scene = importer.ReadFile(path, aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType | aiProcess_Triangulate | aiProcess_GenNormals);
	if( !scene) {
		fprintf( stderr, importer.GetErrorString());
		getchar();
		return false;
	}

	glGenBuffers(1, &asset->tangents);
	glGenBuffers(1, &asset->normals);
	glGenBuffers(1, &asset->uvs);
	glGenBuffers(1, &asset->vertices);
	glGenBuffers(1, &asset->indices);
	glGenVertexArrays(1, &asset->vao);

	// Allocate arrays for each mesh 
	asset->num_materials = scene->mNumMeshes;
	asset->draw_count = (GLint*)malloc( asset->num_materials * sizeof(GLint) );
	asset->draw_start = (GLint*)malloc( asset->num_materials * sizeof(GLint) );
	asset->materials  = (Material**)malloc( asset->num_materials * sizeof(Material*) );
	std::cout << "Number of meshes: " << asset->num_materials << ".\n";

	asset->draw_mode = GL_TRIANGLES;
	asset->draw_type = GL_UNSIGNED_SHORT;

	std::vector<glm::vec3> vertices, normals, tangents;
	std::vector<glm::vec2> uvs;
	std::vector<unsigned short> indices;
	for (int i = 0; i < scene->mNumMeshes; ++i) {
		const aiMesh* mesh = scene->mMeshes[i]; 

		asset->draw_start[i] = indices.size();
		std::cout << "From " << asset->draw_start[i] << " To " << asset->draw_start[i]+3*mesh->mNumFaces-1 << "\n";
		asset->draw_count[i] = 3*mesh->mNumFaces;

		vertices.reserve(mesh->mNumVertices);
		for(unsigned int i=0; i<mesh->mNumVertices; i++){
			auto v = mesh->mVertices[i];
			vertices.push_back(glm::vec3(v.x, v.y, v.z));
		}
		if(mesh->mTextureCoords[0] != NULL){
			uvs.reserve(mesh->mNumVertices);
			for(unsigned int i=0; i<mesh->mNumVertices; i++){
				// Assume only 1 set of UV coords; AssImp supports 8 UV sets.
				auto uvw = mesh->mTextureCoords[0][i];
				uvs.push_back(glm::vec2(uvw.x, uvw.y));
			}
		}
		if(mesh->mNormals != NULL){
			normals.reserve(mesh->mNumVertices);
			for(unsigned int i=0; i<mesh->mNumVertices; i++){
				auto n = mesh->mNormals[i];
				normals.push_back(glm::vec3(n.x, n.y, n.z));
			}
		}

		if(mesh->mTangents != NULL){
			tangents.reserve(mesh->mNumVertices);
			for(unsigned int i=0; i<mesh->mNumVertices; i++){
				auto t = mesh->mTangents[i];
				tangents.push_back(glm::vec3(t.x, t.y, t.z));
			}
		}
		indices.reserve(3*mesh->mNumFaces);
		for (unsigned int i=0; i<mesh->mNumFaces; i++){
			// Assume the model has only triangles.
			indices.push_back(mesh->mFaces[i].mIndices[0]);
			indices.push_back(mesh->mFaces[i].mIndices[1]);
			indices.push_back(mesh->mFaces[i].mIndices[2]);
		}


		auto ai_mat = scene->mMaterials[scene->mMeshes[i]->mMaterialIndex];
		asset->materials[i] = new Material;
		auto mat = asset->materials[i];
		aiColor3D ambient,diffuse;
		if(ai_mat->Get(AI_MATKEY_COLOR_AMBIENT,ambient) == AI_SUCCESS){
			mat->ambient[0] = ambient.r;
			mat->ambient[1] = ambient.g;
			mat->ambient[2] = ambient.b;
		}
		mat->t_ambient = loadTextureFromAssimp(ai_mat, aiTextureType_AMBIENT);
		if(mat->t_ambient == 0) mat->t_ambient = default_material->t_ambient;

		if(ai_mat->Get(AI_MATKEY_COLOR_DIFFUSE,diffuse) == AI_SUCCESS){
			mat->diffuse[0] = diffuse.r;
			mat->diffuse[1] = diffuse.g;
			mat->diffuse[2] = diffuse.b;
		}
		mat->t_diffuse = loadTextureFromAssimp(ai_mat, aiTextureType_DIFFUSE);
		if(mat->t_diffuse == 0) mat->t_diffuse = default_material->t_diffuse;

		// @note Since mtl files specify normals as bump maps assume all bump maps are really normal
		mat->t_normal = loadTextureFromAssimp(ai_mat, aiTextureType_NORMALS);
		if(mat->t_normal == 0) mat->t_normal = loadTextureFromAssimp(ai_mat, aiTextureType_HEIGHT);
		if(mat->t_normal == 0) mat->t_normal = default_material->t_normal;
		
		float spec_exp, spec_int;
		if(ai_mat->Get(AI_MATKEY_SHININESS,spec_exp) == AI_SUCCESS) mat->spec_exp = spec_exp;
		if(ai_mat->Get(AI_MATKEY_SHININESS_STRENGTH,spec_int) == AI_SUCCESS) mat->spec_int = spec_int;
	}

	// bind the VAO
	glBindVertexArray(asset->vao);

	// Load it into a VBO
	glBindBuffer(GL_ARRAY_BUFFER, asset->vertices);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), &vertices[0], GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, asset->uvs);
	glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2), &uvs[0], GL_STATIC_DRAW);
	glVertexAttribPointer(1, 2, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ARRAY_BUFFER, asset->normals);
	glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), &normals[0], GL_STATIC_DRAW);
	glVertexAttribPointer(2, 3, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(2);

	glBindBuffer(GL_ARRAY_BUFFER, asset->tangents);
	glBufferData(GL_ARRAY_BUFFER, tangents.size() * sizeof(glm::vec3), &tangents[0], GL_STATIC_DRAW);
	glVertexAttribPointer(3, 3, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(3);

	// Generate a buffer for the indices as well
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, asset->indices);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);

	glBindVertexArray(0); //Unbind the VAO
	
	// The "scene" pointer will be deleted automatically by "importer"
	return true;
}

GLuint loadTextureFromAssimp(aiMaterial *mat, aiTextureType texture_type){
	// @note spec combines texture stack with operations
	aiString path;
	if(aiReturn_SUCCESS == mat->GetTexture(texture_type, 0, &path))
	{
		auto p = std::string((char *)path.data);
		return loadImage(p);
	}
	return 0;
}
