#include <iostream>

#include <glm/glm.hpp>

#include <controls/globals.hpp>
#include <globals.hpp>

namespace Controls {
	State game;
	State editor;

  glm::dvec2 scroll_offset;
  bool scrolled;
  glm::dvec2 mouse_position;
  glm::dvec2 delta_mouse_position;

	void windowScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
		if (scroll_offset.x != xoffset || scroll_offset.y != yoffset) scrolled = true;
		else scrolled = false;

		scroll_offset.x = xoffset;
		scroll_offset.y = yoffset;
	}
	static void windowCursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
		glm::dvec2 new_mouse_position(xpos, ypos);
		delta_mouse_position = new_mouse_position - mouse_position;
		mouse_position = new_mouse_position;
	}

	void registerCallbacks() {
		glfwSetScrollCallback(window, windowScrollCallback);

		// Set mouse position intially so delta is correct
		glfwGetCursorPos(window, &mouse_position.x, &mouse_position.y);
		glfwSetCursorPosCallback(window, windowCursorPosCallback);
	}
};
