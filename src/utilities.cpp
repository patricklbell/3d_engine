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

