#ifndef CAMERA_GLOBALS_HPP
#define CAMERA_GLOBALS_HPP

#include <camera/core.hpp>

namespace Cameras {
	extern Camera editor_camera;
	extern Camera level_camera;
	extern Camera game_camera;

	Camera* get_active_camera();
};

#endif // CAMERA_GLOBALS_HPP
