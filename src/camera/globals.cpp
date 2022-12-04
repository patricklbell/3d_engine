#include <camera/core.hpp>
#include <camera/globals.hpp>

#include "globals.hpp"
#include "editor.hpp"

namespace Cameras {
	Camera editor_camera = Camera();
	Camera level_camera = Camera();
	Camera game_camera = Camera();

	Camera* get_active_camera() {
		if (playing) {
			return &Cameras::game_camera;
		}
		else if (editor::use_level_camera) {
			return &Cameras::level_camera;
		}
		else {
			return &Cameras::editor_camera;
		}
	}
};