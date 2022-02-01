#if(MSVC AND NOT "${MSVC_VERSION}" LESS 1400)
#	add_definitions( "/MP" )
#endif()


add_definitions(
	-DTW_STATIC
	-DTW_NO_LIB_PRAGMA
	-DTW_NO_DIRECT3D
	-DGLEW_STATIC
	-D_CRT_SECURE_NO_WARNINGS
)

### GLFW ###

add_subdirectory (glfw-3.2.1)

include_directories(
	glfw-3.2.1/include/GLFW/
	glew-2.1.0/include/
)

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
set(OPENGL_LIBRARY
	${OPENGL_LIBRARY}
	-lGL -lGLU -lXrandr -lXext -lX11 -lrt
	${CMAKE_DL_LIBS}
	${GLFW_LIBRARIES}
)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
set(OPENGL_LIBRARY
	${OPENGL_LIBRARY}
	${CMAKE_DL_LIBS}
	${GLFW_LIBRARIES}
)
endif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")

### GLEW ###

set(GLEW_SOURCE
	glew-2.1.0/src/glew.c
)

set(GLEW_HEADERS
)


add_library( GLEW_210 STATIC
	${GLEW_SOURCE}
	${GLEW_INCLUDE}
)

target_link_libraries(GLEW_210
	${OPENGL_LIBRARY}
	${EXTRA_LIBS}
)


### ASSIMP ###
# AssImp already has a CMakeLists.txt so let's use these

# Compile built-in, modified version of Zlib
include(CheckIncludeFile)
include(CheckTypeSize)
include(CheckFunctionExists)
add_subdirectory( assimp-3.0.1270/contrib/zlib )

# Compile without Boost
include_directories( assimp-3.0.1270/code/BoostWorkaround )
add_definitions( -DASSIMP_BUILD_BOOST_WORKAROUND )

# Compile AssImp
set( LIB_INSTALL_DIR "lib")
set(LIBASSIMP_COMPONENT libassimp3.0-r1270-OGLtuts)
set(ZLIB_LIBRARIES zlib)
set(BUILD_STATIC_LIB ON)
#set(ZLIB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/zlib)
add_subdirectory( assimp-3.0.1270/code )

### BULLET ###
# Bullet already has a CMakeLists.txt so let's use these

set(BULLET_VERSION 2.82)
include_directories(
	bullet-2.82-r2704/src
)
add_subdirectory( bullet-2.82-r2704/src/BulletSoftBody )
add_subdirectory( bullet-2.82-r2704/src/BulletCollision )
add_subdirectory( bullet-2.82-r2704/src/BulletDynamics )
add_subdirectory( bullet-2.82-r2704/src/LinearMath )
