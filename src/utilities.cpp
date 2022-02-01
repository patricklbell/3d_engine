#include <unistd.h>
#include <iostream>
#include <filesystem>
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

void ScreenPosToWorldRay(
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

void load_asset(ModelAsset &asset, std::string path){
    auto bmp_path = path;
    bmp_path.replace(path.length()-3, 3, "bmp");
    if(access( bmp_path.c_str(), F_OK ) != -1){
        asset.texture = loadBMP_custom(bmp_path);
    } else {
        std::cout << "No bmp file located for " << bmp_path;
    }

    // Read our .obj file
    std::vector<unsigned short> indices;
    std::vector<glm::vec3> indexed_vertices;
    std::vector<glm::vec2> indexed_uvs;
    std::vector<glm::vec3> indexed_normals;
    bool res = loadAssImp(path, indices, indexed_vertices, indexed_uvs, indexed_normals);
    asset.drawCount = indices.size();

    glGenBuffers(1, &asset.normals);
    glGenBuffers(1, &asset.uvs);
    glGenBuffers(1, &asset.vertices);
    glGenBuffers(1, &asset.indices);
    glGenVertexArrays(1, &asset.vao);

    // bind the VAO
    glBindVertexArray(asset.vao);

    // Load it into a VBO
    glBindBuffer(GL_ARRAY_BUFFER, asset.vertices);
    glBufferData(GL_ARRAY_BUFFER, indexed_vertices.size() * sizeof(glm::vec3), &indexed_vertices[0], GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, 0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, asset.uvs);
    glBufferData(GL_ARRAY_BUFFER, indexed_uvs.size() * sizeof(glm::vec2), &indexed_uvs[0], GL_STATIC_DRAW);
    glVertexAttribPointer(1, 2, GL_FLOAT, false, 0, 0);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, asset.normals);
    glBufferData(GL_ARRAY_BUFFER, indexed_normals.size() * sizeof(glm::vec3), &indexed_normals[0], GL_STATIC_DRAW);
    glVertexAttribPointer(2, 3, GL_FLOAT, false, 0, 0);
    glEnableVertexAttribArray(2);

    // Generate a buffer for the indices as well
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, asset.indices);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);

    glBindVertexArray(0); //Unbind the VAO
}
