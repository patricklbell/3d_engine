#ifndef CAMERA_GLOBALS_HPP
#define CAMERA_GLOBALS_HPP

#include <camera/core.hpp>

namespace Cameras {
	Camera* get_active_camera();
	void update_cameras_for_level();
};

#endif // CAMERA_GLOBALS_HPP
