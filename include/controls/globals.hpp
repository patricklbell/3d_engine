#ifndef ENGINE_CONTROLS_GLOBALS_HPP
#define ENGINE_CONTROLS_GLOBALS_HPP

#include <controls/core.hpp>
#include <glm/glm.hpp>

namespace Controls {
	extern State game;
	extern State editor;

	extern glm::dvec2 scroll_offset;
	extern bool scrolled;
	extern glm::dvec2 mouse_position;
	extern glm::dvec2 delta_mouse_position;

	void registerCallbacks();
};

#endif // ENGINE_CONTROLS_GLOBALS_HPP
