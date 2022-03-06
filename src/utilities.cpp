#include <stdexcept>
#include <unistd.h>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "utilities.hpp"
#include "objloader.hpp"
#include "texture.hpp"
#include "vboindexer.hpp"

void mat4_to_float_array(glm::mat4 i_mat, float o_array[16]) {
    const float* pSource = (const float*)glm::value_ptr(i_mat);
    for (int i = 0; i < 16; ++i)
        o_array[i] = pSource[i];
}
void float_array_to_mat4(glm::mat4& o_mat, float i_array[16]) {
    o_mat = glm::mat4(
                i_array[0],  i_array[1],  i_array[2],  i_array[3],
                i_array[4],  i_array[5],  i_array[6],  i_array[7],
                i_array[8],	 i_array[9],  i_array[10], i_array[11],
                i_array[12], i_array[13], i_array[14], i_array[15]
            );
}

GBuffer generate_gbuffer(unsigned int windowWidth, unsigned int windowHeight){
    GBuffer gb;
    // Create the FBO
    glGenFramebuffers(1, &gb.fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gb.fbo);

    // Create the gbuffer textures
    glGenTextures(GBuffer::GBUFFER_NUM_TEXTURES, gb.textures);
    glGenTextures(1, &gb.depthTexture);

    for (unsigned int i = 0 ; i < GBuffer::GBUFFER_NUM_TEXTURES ; i++) {
        glBindTexture(GL_TEXTURE_2D, gb.textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, windowWidth, windowHeight, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, gb.textures[i], 0);
    }

    // depth
    glBindTexture(GL_TEXTURE_2D, gb.depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, windowWidth, windowHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gb.depthTexture, 0);

    GLenum DrawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
    glDrawBuffers(GBuffer::GBUFFER_NUM_TEXTURES, DrawBuffers);

    GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (Status != GL_FRAMEBUFFER_COMPLETE) {
        printf("status: 0x%x\n", Status);
        throw std::runtime_error("Framebuffer failed to generate");
    }

    // restore default FBO
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    return gb;
}

void screen_pos_to_world_ray(
    int mouseX, int mouseY,             // Mouse position, in pixels, from bottom-left corner of the window
    int screenWidth, int screenHeight,  // Window size, in pixels
    glm::mat4 ViewMatrix,               // Camera position and orientation
    glm::mat4 ProjectionMatrix,         // Camera parameters (ratio, field of view, near and far planes)
    glm::vec3& out_origin,              // Ouput : Origin of the ray. /!\ Starts at the near plane, so if you want the ray to start at the camera's position instead, ignore this.
    glm::vec3& out_direction            // Ouput : Direction, in world space, of the ray that goes "through" the mouse.
) {

    // The ray Start and End positions, in Normalized Device Coordinates (Have you read Tutorial 4 ?)
    glm::vec4 lRayStart_NDC(
        ((float)mouseX / (float)screenWidth - 0.5f) * 2.0f, // [0,1024] -> [-1,1]
        -((float)mouseY / (float)screenHeight - 0.5f) * 2.0f, // [0, 768] -> [-1,1]
        -1.0, // The near plane maps to Z=-1 in Normalized Device Coordinates
        1.0f
    );
    glm::vec4 lRayEnd_NDC(
        ((float)mouseX / (float)screenWidth - 0.5f) * 2.0f,
        -((float)mouseY / (float)screenHeight - 0.5f) * 2.0f,
        0.0,
        1.0f
    );
    // Inverse of projection and view to obtain NDC to world
    glm::mat4 M = glm::inverse(ProjectionMatrix * ViewMatrix);
    glm::vec4 lRayStart_world = M * lRayStart_NDC;
    lRayStart_world/=lRayStart_world.w;
    glm::vec4 lRayEnd_world   = M * lRayEnd_NDC  ;
    lRayEnd_world  /=lRayEnd_world.w;


    glm::vec3 lRayDir_world(lRayEnd_world - lRayStart_world);
    lRayDir_world = glm::normalize(lRayDir_world);


    out_origin = glm::vec3(lRayStart_world);
    out_direction = glm::normalize(lRayDir_world);
}
// Simplified MTL loader which assumes one material
// TODO: illum model
bool load_mtl(Material * mat, const std::string &path){
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
            ss >> mat->transFilter[0];
            if(!(ss >> mat->transFilter[1])){
                mat->transFilter[1] = mat->transFilter[0];
                mat->transFilter[2] = mat->transFilter[0];
            }
        } else if (head == "d") {
            ss >> mat->dissolve;
        } else if (head == "Ns") {
            ss >> mat->specExp;
        } else if (head == "sharpness") {
            ss >> mat->reflectSharp;
        } else if (head == "Ni") {
            ss >> mat->opticDensity;
        } else if (head == "map_Ka") {
            // TODO: handle arguments
            auto index = line.find_last_of(' ');
            std::string tex_path = line.substr(++index);
            mat->tAlbedo = loadImage(tex_path);
        } else if (head == "map_Kd") {
            // TODO: handle arguments
            auto index = line.find_last_of(' ');
            std::string tex_path = line.substr(++index);
            mat->tDiffuse = loadImage(tex_path);
        } else if (head == "norm" || head == "map_Bump") {
            // TODO: handle arguments
            auto index = line.find_last_of(' ');
            std::string tex_path = line.substr(++index);
            mat->tNormal = loadImage(tex_path);
	    }
    }

	return true;
}

void load_asset(ModelAsset * asset, const std::string &objpath, const std::string &mtlpath){
	if(!load_mtl(asset->mat, mtlpath)){
		std::cout << "Could not load MTL: " << mtlpath << "\n";
	}

    // Read our .obj file
    std::vector<unsigned short> indices;
    std::vector<glm::vec3> indexed_vertices;
    std::vector<glm::vec2> indexed_uvs;
    std::vector<glm::vec3> indexed_normals;
    std::vector<glm::vec3> indexed_tangents;
    bool res = loadAssImp(objpath, indices, indexed_vertices, indexed_uvs, indexed_normals, indexed_tangents);
    asset->drawCount = indices.size();

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
