#include <camera/core.hpp>
#include <camera/globals.hpp>

#include "globals.hpp"
#include "editor.hpp"
#include "game_behaviour.hpp"

namespace Cameras {
	Camera* get_active_camera() {
		if (gamestate.is_active) {
			return &gamestate.level.camera;
		}
		else if (Editor::use_level_camera) {
			return &loaded_level.camera;
		}
		else {
			return &Editor::editor_camera;
		}
	}

	void update_cameras_for_level() {
		Editor::editor_camera = loaded_level.camera;
		Editor::editor_camera.state = Camera::TRACKBALL;
	}
};