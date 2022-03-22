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

#include "texture.hpp"
#include "assets.hpp"

bool loadAssimp(
	std::string path, 
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
	const aiMesh* mesh = scene->mMeshes[0]; // In this simple example code we always use the 1st mesh (in OBJ files there is often only one anyway)

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

	// Fill face indices
	indices.reserve(3*mesh->mNumFaces);
	for (unsigned int i=0; i<mesh->mNumFaces; i++){
		// Assume the model has only triangles.
		indices.push_back(mesh->mFaces[i].mIndices[0]);
		indices.push_back(mesh->mFaces[i].mIndices[1]);
		indices.push_back(mesh->mFaces[i].mIndices[2]);
	}
	
	// The "scene" pointer will be deleted automatically by "importer"
	return true;
}

// MTL loader which assumes one material
// TODO: illum model
bool loadMtl(Material * mat, const std::string &path){
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
	while( std::getline(file, line) ){
		std::istringstream ss(line);
		std::string head;
		ss >> head;
		
		if(head == "newmtl"){
			ss >> mat->name;	
		} else if (head == "Kd") {
			ss >> mat->albedo[0];
			if(!(ss >> mat->albedo[1])){
				mat->albedo[1] = mat->albedo[0];
				mat->albedo[2] = mat->albedo[0];
			}
		} else if (head == "Ka") {
			ss >> mat->diffuse[0];
			if(!(ss >> mat->diffuse[1])){
				mat->diffuse[1] = mat->diffuse[0];
				mat->diffuse[2] = mat->diffuse[0];
			}

		} else if (head == "Ks") {
			ss >> mat->specular[0];
			if(!(ss >> mat->specular[1])){
				mat->specular[1] = mat->specular[0];
				mat->specular[2] = mat->specular[0];
			}
		} else if (head == "Tf") {
			ss >> mat->trans_filter[0];
			if(!(ss >> mat->trans_filter[1])){
				mat->trans_filter[1] = mat->trans_filter[0];
				mat->trans_filter[2] = mat->trans_filter[0];
			}
		} else if (head == "d") {
			ss >> mat->dissolve;
		} else if (head == "Ns") {
			ss >> mat->spec_exp;
		} else if (head == "sharpness") {
			ss >> mat->reflect_sharp;
		} else if (head == "Ni") {
			ss >> mat->optic_density;
		} else if (head == "map_Ka") {
			// TODO: handle arguments
			auto index = line.find_last_of(' ');
			std::string tex_path = line.substr(++index);
			mat->t_albedo = loadImage(directory+tex_path);
		} else if (head == "map_Kd") {
			// TODO: handle arguments
			auto index = line.find_last_of(' ');
			std::string tex_path = line.substr(++index);
			mat->t_diffuse = loadImage(directory+tex_path, true);
		} else if (head == "norm" || head == "map_Bump") {
			// TODO: handle arguments
			auto index = line.find_last_of(' ');
			std::string tex_path = line.substr(++index);
			mat->t_normal = loadImage(directory+tex_path);
		}
	}

	return true;
}

void loadAsset(ModelAsset * asset, const std::string &objpath, const std::string &mtlpath){
	if(mtlpath != ""){
		if(!loadMtl(asset->mat, mtlpath)){
			std::cout << "Could not load MTL: " << mtlpath << "\n";
		}
	}

	// Read our .obj file
	std::vector<unsigned short> indices;
	std::vector<glm::vec3> indexed_vertices;
	std::vector<glm::vec2> indexed_uvs;
	std::vector<glm::vec3> indexed_normals;
	std::vector<glm::vec3> indexed_tangents;
	bool res = loadAssimp(objpath, indices, indexed_vertices, indexed_uvs, indexed_normals, indexed_tangents);
	asset->draw_count = indices.size();

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
}
