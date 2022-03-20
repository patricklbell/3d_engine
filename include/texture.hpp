#ifndef TEXTURE_HPP
#define TEXTURE_HPP

GLuint load_image(std::string imagepath, bool is_srgb=false);

// Load a .BMP file using our custom loader
GLuint loadBMP_custom(std::string imagepath);

//// Since GLFW 3, glfwLoadTexture2D() has been removed. You have to use another texture loading library, 
//// or do it yourself (just like loadImage and loadDDS)
//// Load a .TGA file using GLFW's own loader
//GLuint loadTGA_glfw(const char * imagepath);

// Load a .DDS file using GLFW's own loader
GLuint loadDDS(const char * imagepath);
GLuint load_cubemap(std::vector<std::string> filenames);

#endif
