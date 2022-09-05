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

add_subdirectory (glfw-3.3.7)

include_directories(
	glfw-3.3.7/include/GLFW/
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

set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF)
set(ASSIMP_BUILD_GLTF_IMPORTER ON)
set(ASSIMP_BUILD_OBJ_IMPORTER ON)
set(ASSIMP_BUILD_FBX_IMPORTER ON)
set(ASSIMP_BUILD_COLLADA_IMPORTER ON)
add_subdirectory( assimp-5.1.5 )

### SoLoud ###
cmake_policy(SET CMP0091 NEW)

if(WIN32)
	add_compile_definitions(WITH_WINMM)
elseif(APPLE)
	## @todo
elseif(UNIX)
	add_compile_definitions(WITH_ALSA)
endif()

set(SOLOUD_SOURCE
  soloud20200207/src/audiosource/monotone/soloud_monotone.cpp
  soloud20200207/src/audiosource/noise/soloud_noise.cpp
  soloud20200207/src/audiosource/openmpt/soloud_openmpt.cpp
  soloud20200207/src/audiosource/openmpt/soloud_openmpt_dll.c
  soloud20200207/src/audiosource/sfxr/soloud_sfxr.cpp
  soloud20200207/src/audiosource/speech/darray.cpp
  soloud20200207/src/audiosource/speech/klatt.cpp
  soloud20200207/src/audiosource/speech/resonator.cpp
  soloud20200207/src/audiosource/speech/soloud_speech.cpp
  soloud20200207/src/audiosource/speech/tts.cpp
  soloud20200207/src/audiosource/tedsid/sid.cpp
  soloud20200207/src/audiosource/tedsid/soloud_tedsid.cpp
  soloud20200207/src/audiosource/tedsid/ted.cpp
  soloud20200207/src/audiosource/vic/soloud_vic.cpp
  soloud20200207/src/audiosource/vizsn/soloud_vizsn.cpp
  soloud20200207/src/audiosource/wav/dr_impl.cpp
  soloud20200207/src/audiosource/wav/soloud_wav.cpp
  soloud20200207/src/audiosource/wav/soloud_wavstream.cpp
  soloud20200207/src/audiosource/wav/stb_vorbis.c
  soloud20200207/src/backend/null/soloud_null.cpp
  soloud20200207/src/backend/winmm/soloud_winmm.cpp
  soloud20200207/src/backend/alsa/soloud_alsa.cpp
  soloud20200207/src/core/soloud.cpp
  soloud20200207/src/core/soloud_audiosource.cpp
  soloud20200207/src/core/soloud_bus.cpp
  soloud20200207/src/core/soloud_core_3d.cpp
  soloud20200207/src/core/soloud_core_basicops.cpp
  soloud20200207/src/core/soloud_core_faderops.cpp
  soloud20200207/src/core/soloud_core_filterops.cpp
  soloud20200207/src/core/soloud_core_getters.cpp
  soloud20200207/src/core/soloud_core_setters.cpp
  soloud20200207/src/core/soloud_core_voicegroup.cpp
  soloud20200207/src/core/soloud_core_voiceops.cpp
  soloud20200207/src/core/soloud_fader.cpp
  soloud20200207/src/core/soloud_fft.cpp
  soloud20200207/src/core/soloud_fft_lut.cpp
  soloud20200207/src/core/soloud_file.cpp
  soloud20200207/src/core/soloud_filter.cpp
  soloud20200207/src/core/soloud_misc.cpp
  soloud20200207/src/core/soloud_queue.cpp
  soloud20200207/src/core/soloud_thread.cpp
  soloud20200207/src/filter/soloud_bassboostfilter.cpp
  soloud20200207/src/filter/soloud_biquadresonantfilter.cpp
  soloud20200207/src/filter/soloud_dcremovalfilter.cpp
  soloud20200207/src/filter/soloud_echofilter.cpp
  soloud20200207/src/filter/soloud_fftfilter.cpp
  soloud20200207/src/filter/soloud_flangerfilter.cpp
  soloud20200207/src/filter/soloud_freeverbfilter.cpp
  soloud20200207/src/filter/soloud_lofifilter.cpp
  soloud20200207/src/filter/soloud_robotizefilter.cpp
  soloud20200207/src/filter/soloud_waveshaperfilter.cpp
)


add_library(SOLOUD STATIC 
	${SOLOUD_SOURCE}
	${SOLOUD_INCLUDE}
)

if(UNIX)
	find_package(ALSA REQUIRED)                                    
	if (ALSA_FOUND)                                                
    	include_directories(${ALSA_INCLUDE_DIRS})                  
    	target_link_libraries(SOLOUD ${ALSA_LIBRARIES})      
	endif(ALSA_FOUND)
	add_compile_definitions(WITH_ALSA)
endif()
