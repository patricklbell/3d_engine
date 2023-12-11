#ifndef ENGINE_CAMERA_GLOBALS_HPP
#define ENGINE_CAMERA_GLOBALS_HPP

#include <camera/core.hpp>

namespace Cameras {
	Camera* get_active_camera();
	void update_cameras_for_level();
};

#endif // ENGINE_CAMERA_GLOBALS_HPP
