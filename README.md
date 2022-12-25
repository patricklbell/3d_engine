# 3D Engine
![Crytek's Sponza scene](https://github.com/patricklbell/3d_engine/blob/main/data/screenshots/sponza.png?raw=true)
A rendering and game engine with as many features as I felt like, including:
- Multiple material types (PBR, water, emissive, Blinn-Phong, etc.)[^1]
- Directional lighting with cascaded shadow maps and point lights
- Physically based volumetric lighting based on EA's [extinction volume method](https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite)
- Water shader with dynamic tesselation, and lighting based on [Alex Tardif's](https://alextardif.com/Water.html) and [Jean-Phillipe Grenier's](https://80.lv/articles/river-editor-water-simulation-in-real-time/) articles, this is more of a prototype but water is hard to get right[^2]
- Level, mesh and animation binary serialisation, asynchronous texture loading with [STB_image](https://github.com/nothings/stb)
- Skeletal animation, texture animation, and vegetation geometry animation based on [Crytek's method](https://developer.nvidia.com/gpugems/gpugems3/part-iii-rendering/chapter-16-vegetation-procedural-animation-and-shading-crysis)
- Antialiasing with MSAA and FXAA, PBR bloom, Exposure adjustment and tonemapping, and SSAO (kept subtle because of depth reconstruction imprecisions)
- Lightmapping with [lightmapper](https://github.com/ands/lightmapper), with UV parameterisations done by [XAtlas](https://github.com/jpcy/xatlas)
- Editor with lots of debugging visualisations, uses [Dear ImGui](https://github.com/ocornut/imgui) for the GUI
- Model loading with [Assimp](https://github.com/assimp/assimp), custom GLSL shader pre-processor, dynamic keyboard bindings, player and camera controller, etc.

[^1]: ![Some different material types, Blinn-Phong, PBR dielectric, IBL PBR metallic, emissive](https://github.com/patricklbell/3d_engine/blob/main/data/screenshots/materials.png?raw=true)
  Some of the different material types, from left to right: Blinn-Phong, PBR dielectric, PBR metallic with IBL, emissive. Let me know if there are any errors in the PBR calculations (I doubt it properly conserves energy).

[^2]: ![Water](https://github.com/patricklbell/3d_engine/blob/main/data/screenshots/water.png?raw=true)
  Water with Tesselation shader to create dynamic level of detail.

## Building
All the dependencies are built statically except OpenGL.
### Windows
#### Requirements
You need CMake and the Visual Studio build tool-chain for C++. Additionally, OpenGL
must be at or above version 4.3 (Should already work, if not, update your graphics 
drivers). 
#### Building
Download the source and unzip or 
```
git clone https://github.com/patricklbell/3d_engine.git
```
Navigate to the root of source, create a build directory and build with CMake:
```
cd 3d_engine
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ./..
cmake --build . --config Release
```
You then need to make sure that your working director contains the data folder,
this can be done by copying the data folder to the build directory for example.

### Linux (X11)
#### Requirements
To compile GLFW you need the X11 development packages installed, on Debian and 
derivates like Ubuntu and Linux Mint the xorg-dev meta-package pulls in the 
development packages for all of X11. For more information see 
https://www.glfw.org/docs/3.3/compile.html. You will need CMake, and OpenGL 
must be above version 3.3 (Should already work, if not, update your graphics 
drivers).
#### Building
Download the source and unzip or 
```
git clone https://github.com/patricklbell/3d_engine.git
```
Navigate to the root of source, create a build directory and build with CMake:
```
cd 3d_engine
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ./..
cmake --build .
```
You then need to make sure that your working director contains the data folder,
this can be done by copying the data folder to the build directory for example.

### OSX
You're on your own here, sorry!
