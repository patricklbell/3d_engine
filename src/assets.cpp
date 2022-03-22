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

void initDefaultMaterial(){
    default_material = new Material;
    default_material->t_diffuse = loadImage("data/textures/default_diffuse.bmp");
    default_material->t_normal  = loadImage("data/textures/default_normal.bmp");
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
		getchar();
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

bool loadAssetObj(Asset * asset, const std::string &objpath, const std::string &mtlpath){
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
	asset->num_meshes = mesh_materials.size();
	asset->draw_count = (GLint*)malloc( asset->num_meshes * sizeof(GLint) );
	asset->draw_start = (GLint*)malloc( asset->num_meshes * sizeof(GLint) );
	asset->materials  = (Material**)malloc( asset->num_meshes * sizeof(Material*) );
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
	asset->program_id = shader::geometry;
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

bool loadAsset(Asset * asset, const std::string &path){
	printf("Loading asset %s.\n", path.c_str());

	Assimp::Importer importer;

	const aiScene* scene = importer.ReadFile(path, aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType | aiProcess_Triangulate);
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
	asset->num_meshes = scene->mNumMeshes;
	asset->draw_count = (GLint*)malloc( asset->num_meshes * sizeof(GLint) );
	asset->draw_start = (GLint*)malloc( asset->num_meshes * sizeof(GLint) );
	asset->materials  = (Material**)malloc( asset->num_meshes * sizeof(Material*) );

	asset->program_id = shader::geometry;
	asset->draw_mode = GL_TRIANGLES;
	asset->draw_type = GL_UNSIGNED_SHORT;


	// @note could check if none of the properties is null and disable ie no uvs
	// Calculate size of each buffers
	int n_vertices, n_uvs, n_tangents, n_normals, n_indices;
	for (int i = 0; i < scene->mNumMeshes; ++i) {
		if(scene->mMeshes[i]->mTextureCoords[0] != NULL) n_uvs += scene->mMeshes[i]->mNumVertices;
		if(scene->mMeshes[i]->mTangents != NULL) n_tangents += scene->mMeshes[i]->mNumVertices;
		if(scene->mMeshes[i]->mNormals != NULL) n_normals += scene->mMeshes[i]->mNumVertices;
		if(scene->mMeshes[i]->mVertices == NULL) return false;
		if(scene->mMeshes[i]->mFaces == NULL) return false;
		n_vertices += scene->mMeshes[i]->mNumVertices;
		n_indices  += scene->mMeshes[i]->mNumFaces;
		auto m = scene->mMaterials[scene->mMeshes[i]->mMaterialIndex];
		asset->materials[i] = new Material;
		aiColor3D ambient,diffuse;
		if(m->Get(AI_MATKEY_COLOR_AMBIENT,ambient) == AI_SUCCESS){
			asset->materials[i]->ambient[0] = ambient.r;
			asset->materials[i]->ambient[1] = ambient.g;
			asset->materials[i]->ambient[2] = ambient.b;
		}
		asset->materials[i]->t_ambient = loadTextureFromAssimp(m, aiTextureType_AMBIENT);
		if(m->Get(AI_MATKEY_COLOR_DIFFUSE,diffuse) == AI_SUCCESS){
			asset->materials[i]->diffuse[0] = diffuse.r;
			asset->materials[i]->diffuse[1] = diffuse.g;
			asset->materials[i]->diffuse[2] = diffuse.b;
		}
		asset->materials[i]->t_diffuse = loadTextureFromAssimp(m, aiTextureType_DIFFUSE);
		asset->materials[i]->t_normal  = loadTextureFromAssimp(m, aiTextureType_NORMALS);
		
		float spec_exp, spec_int;
		if(m->Get(AI_MATKEY_SHININESS,spec_exp) == AI_SUCCESS) asset->materials[i]->spec_exp = spec_exp;
		if(m->Get(AI_MATKEY_SHININESS_STRENGTH,spec_int) == AI_SUCCESS) asset->materials[i]->spec_int = spec_int;
	}
	n_vertices *= 3;
	n_indices  *= 3;
	// bind the VAO
	glBindVertexArray(asset->vao);

	// Set out enough memory in buffers
	glBindBuffer(GL_ARRAY_BUFFER, asset->vertices);
	glBufferData(GL_ARRAY_BUFFER, n_vertices * sizeof(float), NULL, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, asset->uvs);
	glBufferData(GL_ARRAY_BUFFER, n_vertices * sizeof(float), NULL, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 2, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ARRAY_BUFFER, asset->normals);
	glBufferData(GL_ARRAY_BUFFER, n_vertices * sizeof(float), NULL, GL_STATIC_DRAW);
	glVertexAttribPointer(2, 3, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(2);

	glBindBuffer(GL_ARRAY_BUFFER, asset->tangents);
	glBufferData(GL_ARRAY_BUFFER, n_vertices * sizeof(float), NULL, GL_STATIC_DRAW);
	glVertexAttribPointer(3, 3, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(3);

	// Generate a buffer for the indices as well
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, asset->indices);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, n_indices * sizeof(unsigned short), NULL, GL_STATIC_DRAW);

	// Actually fill in data
	int vert_offset = 0, indices_offset = 0;
	for (int i = 0; i < scene->mNumMeshes; ++i) {
		const aiMesh* mesh = scene->mMeshes[i]; 

		glBindBuffer(GL_ARRAY_BUFFER, asset->vertices);
		for(unsigned int i=0; i<mesh->mNumVertices; i++){
			glBufferSubData(GL_ARRAY_BUFFER, (3*(vert_offset+i))  *sizeof(float), sizeof(float), &mesh->mVertices[i].x);
			glBufferSubData(GL_ARRAY_BUFFER, (3*(vert_offset+i)+1)*sizeof(float), sizeof(float), &mesh->mVertices[i].y);
			glBufferSubData(GL_ARRAY_BUFFER, (3*(vert_offset+i)+2)*sizeof(float), sizeof(float), &mesh->mVertices[i].z);
		}
		if(mesh->mTextureCoords[0] != NULL){
			glBindBuffer(GL_ARRAY_BUFFER, asset->uvs);
			for(unsigned int i=0; i<mesh->mNumVertices; i++){
				// Assume only 1 set of UV coords; AssImp supports 8 UV sets.
				glBufferSubData(GL_ARRAY_BUFFER, (2*(vert_offset+i))  *sizeof(float), sizeof(float), &mesh->mTextureCoords[0][i].x);
				glBufferSubData(GL_ARRAY_BUFFER, (2*(vert_offset+i)+1)*sizeof(float), sizeof(float), &mesh->mTextureCoords[0][i].y);
			}
		}
		if(mesh->mNormals != NULL){
			glBindBuffer(GL_ARRAY_BUFFER, asset->normals);
			for(unsigned int i=0; i<mesh->mNumVertices; i++){
				glBufferSubData(GL_ARRAY_BUFFER, (3*(vert_offset+i))  *sizeof(float), sizeof(float), &mesh->mNormals[i].x);
				glBufferSubData(GL_ARRAY_BUFFER, (3*(vert_offset+i)+1)*sizeof(float), sizeof(float), &mesh->mNormals[i].y);
				glBufferSubData(GL_ARRAY_BUFFER, (3*(vert_offset+i)+2)*sizeof(float), sizeof(float), &mesh->mNormals[i].z);
			}
		}
		if(mesh->mTangents != NULL){
			glBindBuffer(GL_ARRAY_BUFFER, asset->tangents);
			for(unsigned int i=0; i<mesh->mNumVertices; i++){
				glBufferSubData(GL_ARRAY_BUFFER, (3*(vert_offset+i))  *sizeof(float), sizeof(float), &mesh->mTangents[i].x);
				glBufferSubData(GL_ARRAY_BUFFER, (3*(vert_offset+i)+1)*sizeof(float), sizeof(float), &mesh->mTangents[i].y);
				glBufferSubData(GL_ARRAY_BUFFER, (3*(vert_offset+i)+2)*sizeof(float), sizeof(float), &mesh->mTangents[i].z);
			}
		}
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, asset->indices);
		for(unsigned int i=0; i<mesh->mNumFaces; i++){
			glBufferSubData(GL_ARRAY_BUFFER, (3*(indices_offset+i))*sizeof(unsigned short), 3*sizeof(unsigned short), &mesh->mFaces[i].mIndices[0]);
		}

		vert_offset    += mesh->mNumVertices;
		indices_offset += mesh->mNumFaces;
	}
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
