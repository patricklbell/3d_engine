#include <vector>
#include <stdio.h>
#include <string>
#include <cstring>
#include <cmath>
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
    default_material->t_albedo = create1x1Texture(albedo);

    unsigned char normal[] = {128, 127, 255};
    default_material->t_normal = create1x1Texture(normal);

    // Note GL_FALSE may work so texture is set to nothing, either way
    // loading three of the same texture is needless, set them equal if
    // correct values
    unsigned char metallic[] = {0, 0, 0};
    default_material->t_metallic = create1x1Texture(metallic, GL_RGB);

    unsigned char roughness[] = {0, 0, 0};
    default_material->t_roughness = create1x1Texture(roughness, GL_RGB);

    unsigned char ambient[] = {1, 0, 0};
    default_material->t_ambient = create1x1Texture(ambient, GL_SRGB);
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

bool loadAsset(Mesh * mesh, const std::string &path){
	printf("--------------------Loading Asset %s--------------------\n", path.c_str());

	Assimp::Importer importer;

	const aiScene* scene = importer.ReadFile(path, ai_import_flags);
	if( !scene) {
		fprintf( stderr, importer.GetErrorString());
		getchar();
		return false;
	}

	//glGenBuffers(1, &mesh->bitangents);
	glGenBuffers(1, &mesh->tangents);
	glGenBuffers(1, &mesh->normals);
	glGenBuffers(1, &mesh->uvs);
	glGenBuffers(1, &mesh->vertices);
	glGenBuffers(1, &mesh->indices);
	glGenVertexArrays(1, &mesh->vao);

	// Allocate arrays for each mesh 
	mesh->num_materials = scene->mNumMeshes;
	mesh->draw_count = (GLint*)malloc( mesh->num_materials * sizeof(GLint) );
	mesh->draw_start = (GLint*)malloc( mesh->num_materials * sizeof(GLint) );
	mesh->materials  = (Material**)malloc( mesh->num_materials * sizeof(Material*) );
	std::cout << "Number of meshes: " << mesh->num_materials << ".\n";

	mesh->draw_mode = GL_TRIANGLES;
	mesh->draw_type = GL_UNSIGNED_SHORT;

	std::vector<glm::vec3> vertices, normals, tangents;
    //std::vector<glm::vec3> bitangents;
	std::vector<glm::vec2> uvs;
	std::vector<unsigned short> indices;
	for (int i = 0; i < scene->mNumMeshes; ++i) {
		const aiMesh* ai_mesh = scene->mMeshes[i]; 

		mesh->draw_start[i] = indices.size();
		mesh->draw_count[i] = 3*ai_mesh->mNumFaces;

        printf("Loading mesh index %d from face %d ---> %d.\n", i, mesh->draw_start[i], mesh->draw_start[i]+3*ai_mesh->mNumFaces-1);

		vertices.reserve(ai_mesh->mNumVertices);
		for(unsigned int i=0; i<ai_mesh->mNumVertices; i++){
			auto v = ai_mesh->mVertices[i];
			vertices.push_back(glm::vec3(v.x, v.y, v.z));
		}
		if(ai_mesh->mTextureCoords[0] != NULL){
			uvs.reserve(ai_mesh->mNumVertices);
			for(unsigned int i=0; i<ai_mesh->mNumVertices; i++){
				// Assume only 1 set of UV coords; AssImp supports 8 UV sets.
				auto uvw = ai_mesh->mTextureCoords[0][i];
				uvs.push_back(glm::vec2(uvw.x, uvw.y));
			}
		}
		if(ai_mesh->mNormals != NULL){
			normals.reserve(ai_mesh->mNumVertices);
			for(unsigned int i=0; i<ai_mesh->mNumVertices; i++){
				auto n = ai_mesh->mNormals[i];
				normals.push_back(glm::vec3(n.x, n.y, n.z));
			}
		}

		if(ai_mesh->mTangents != NULL){
			tangents.reserve(ai_mesh->mNumVertices);
			for(unsigned int i=0; i<ai_mesh->mNumVertices; i++){
				auto t = ai_mesh->mTangents[i];
				tangents.push_back(glm::vec3(t.x, t.y, t.z));
			}
		}
		
		//if(mesh->mBitangents != NULL){
		//	bitangents.reserve(mesh->mNumVertices);
		//	for(unsigned int i=0; i<mesh->mNumVertices; i++){
		//		auto bt = mesh->mBitangents[i];
		//		bitangents.push_back(glm::vec3(bt.x, bt.y, bt.z));
		//	}
		//}

		indices.reserve(3*ai_mesh->mNumFaces);
		for (unsigned int i=0; i<ai_mesh->mNumFaces; i++){
			// Assumes the model has only triangles.
			indices.push_back(ai_mesh->mFaces[i].mIndices[0]);
			indices.push_back(ai_mesh->mFaces[i].mIndices[1]);
			indices.push_back(ai_mesh->mFaces[i].mIndices[2]);
		}


        // Load material from assimp, materials are wrong because assimp doesnt
        // support PBR materials
		auto ai_mat = scene->mMaterials[scene->mMeshes[i]->mMaterialIndex];
		mesh->materials[i] = new Material;
		auto mat = mesh->materials[i];

		mat->t_ambient = loadTextureFromAssimp(ai_mat, scene, aiTextureType_AMBIENT_OCCLUSION, GL_SRGB);
        if(mat->t_ambient == 0) mat->t_ambient = loadTextureFromAssimp(ai_mat, scene, aiTextureType_AMBIENT, GL_SRGB);
        if(mat->t_ambient == 0){
            aiColor3D ambient;
            if(ai_mat->Get(AI_MATKEY_COLOR_AMBIENT,ambient) == AI_SUCCESS){

                unsigned char data[] = {
                    (unsigned char)floor(ambient.r*255),
                    (unsigned char)floor(ambient.g*255),
                    (unsigned char)floor(ambient.b*255),
                };
                mat->t_ambient = create1x1Texture(data, GL_SRGB);
            }
        }
		if(mat->t_ambient == 0) mat->t_ambient = default_material->t_ambient;

		mat->t_albedo = loadTextureFromAssimp(ai_mat, scene, aiTextureType_BASE_COLOR, GL_SRGB);
        // If base color isnt present assume diffuse is really an albedo
		if(mat->t_albedo == 0) mat->t_albedo = loadTextureFromAssimp(ai_mat, scene, aiTextureType_DIFFUSE, GL_SRGB);
        if(mat->t_albedo == 0){
            aiColor3D albedo;
            if(ai_mat->Get(AI_MATKEY_COLOR_AMBIENT,albedo) == AI_SUCCESS){
                unsigned char data[] = {
                    (unsigned char)floor(albedo.r*255),
                    (unsigned char)floor(albedo.g*255),
                    (unsigned char)floor(albedo.b*255),
                };
                mat->t_albedo = create1x1Texture(data, GL_SRGB);
            }
        }
		if(mat->t_albedo == 0) mat->t_albedo = default_material->t_albedo;

		mat->t_metallic = loadTextureFromAssimp(ai_mat, scene, aiTextureType_METALNESS, GL_RGB);
        if(mat->t_metallic == 0){
            aiColor3D metallic;
		    if(ai_mat->Get(AI_MATKEY_METALLIC_FACTOR,metallic) == AI_SUCCESS){
                unsigned char data[] = {
                    (unsigned char)floor(metallic.r*255),
                    (unsigned char)floor(metallic.g*255),
                    (unsigned char)floor(metallic.b*255),
                };
                mat->t_metallic = create1x1Texture(data, GL_RGB);
            }
        }
		if(mat->t_metallic == 0) mat->t_metallic = default_material->t_metallic;

		mat->t_roughness = loadTextureFromAssimp(ai_mat, scene, aiTextureType_DIFFUSE_ROUGHNESS, GL_RGB);
        if(mat->t_roughness == 0){
            aiColor3D roughness;
		    if(ai_mat->Get(AI_MATKEY_ROUGHNESS_FACTOR,roughness) == AI_SUCCESS){
                unsigned char data[] = {
                    (unsigned char)floor(roughness.r*255),
                    (unsigned char)floor(roughness.g*255),
                    (unsigned char)floor(roughness.b*255),
                };
                mat->t_roughness = create1x1Texture(data, GL_RGB);
            }
        }
		if(mat->t_roughness == 0) mat->t_roughness = default_material->t_roughness;

		// @note Since mtl files specify normals as bump maps assume all bump maps are really normal
		mat->t_normal = loadTextureFromAssimp(ai_mat, scene, aiTextureType_NORMALS, GL_RGB);
		if(mat->t_normal == 0) mat->t_normal = loadTextureFromAssimp(ai_mat, scene, aiTextureType_HEIGHT, GL_RGB);
		if(mat->t_normal == 0) mat->t_normal = default_material->t_normal;
	}

	// bind the vao for writing vbos
	glBindVertexArray(mesh->vao);

	// Load the packed vector data into a VBO
	glBindBuffer(GL_ARRAY_BUFFER, mesh->vertices);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), &vertices[0], GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, mesh->uvs);
	glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2), &uvs[0], GL_STATIC_DRAW);
	glVertexAttribPointer(1, 2, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ARRAY_BUFFER, mesh->normals);
	glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), &normals[0], GL_STATIC_DRAW);
	glVertexAttribPointer(2, 3, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(2);

	glBindBuffer(GL_ARRAY_BUFFER, mesh->tangents);
	glBufferData(GL_ARRAY_BUFFER, tangents.size() * sizeof(glm::vec3), &tangents[0], GL_STATIC_DRAW);
	glVertexAttribPointer(3, 3, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(3);

	//glBindBuffer(GL_ARRAY_BUFFER, asset->bitangents);
	//glBufferData(GL_ARRAY_BUFFER, bitangents.size() * sizeof(glm::vec3), &bitangents[0], GL_STATIC_DRAW);
	//glVertexAttribPointer(3, 3, GL_FLOAT, false, 0, 0);
	//glEnableVertexAttribArray(4);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->indices);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);

	glBindVertexArray(0); //Unbind the VAO
	
	// The "scene" pointer will be deleted automatically by "importer"
	return true;
}

GLuint loadTextureFromAssimp(aiMaterial *mat, const aiScene *scene, aiTextureType texture_type, GLint internal_format=GL_SRGB){
	// @note assimp specification wants us to combines a stack texture stack with operations per layer
	aiString path;
	if(aiReturn_SUCCESS == mat->GetTexture(texture_type, 0, &path))
	{
        auto tex = scene->GetEmbeddedTexture(path.C_Str());
        // If true this is an embedded texture so load from assimp
        if(tex != nullptr){
            printf("Loading embedded texture %s.\n", path.C_Str());
            GLuint texture_id;
            
            glBindTexture(GL_TEXTURE_2D, texture_id);// Binding of texture name
			// We will use linear interpolation for magnification filter
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			// tiling mode
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (tex->achFormatHint[0] & 0x01) ? GL_REPEAT : GL_CLAMP);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (tex->achFormatHint[0] & 0x01) ? GL_REPEAT : GL_CLAMP);
			// Texture specification
			glTexImage2D(GL_TEXTURE_2D, 0, internal_format, tex->mWidth, tex->mHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, tex->pcData);
            return texture_id;
        } else {
            auto p = std::string((char *)path.data);
            return loadImage(p, internal_format);
        }
	}
	return 0;
}
