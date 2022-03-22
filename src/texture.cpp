#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#define cimg_display 0
#include "CImg.h"
using namespace cimg_library;

#include <GL/glew.h>

#include <GLFW/glfw3.h>

GLuint loadImage(std::string imagepath, bool is_srgb=false){
	printf("Loading texture: %s\n", imagepath.c_str());
	CImg<unsigned char> src(imagepath.c_str());
	int w = src.width();
	int h = src.height();
	//printf("width: %d, height: %d, size: %d, depth: %d\n",src.width(), src.height(), (int)src.size(), src.depth() );

	src.permute_axes("cxyz");

	// Create one OpenGL texture
	GLuint texture_id;
	glGenTextures(1, &texture_id);
	
	// "Bind" the newly created texture : all future texture functions will modify this texture
	glBindTexture(GL_TEXTURE_2D, texture_id);

	// Give the image to OpenGL
	if(is_srgb)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, src.data());
	else
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, src.data());

	// Poor filtering, or ...
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 

	// ... nice trilinear filtering ...
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	// ... which requires mipmaps. Generate them automatically.
	glGenerateMipmap(GL_TEXTURE_2D);

	// Return the ID of the texture we just created
	return texture_id;
}

GLuint loadCubemap(std::vector<std::string> filenames){
    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture_id);

    for(unsigned int i = 0; i < filenames.size(); i++)
    {
        CImg<unsigned char> src(filenames[i].c_str());
        int w = src.width();
        int h = src.height();
        src.permute_axes("cxyz");
        glTexImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
            0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, src.data()
        );
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	return texture_id;
}
