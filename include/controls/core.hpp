#ifndef CONTROLS_CORE_HPP
#define CONTROLS_CORE_HPP

#include <unordered_map>
#include <string>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <globals.hpp>

namespace Controls {
	void windowScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

	enum ButtonState {
		RELEASED = 1,
		HELD = 1 << 1,
		PRESS = 1 << 2 | HELD,
		RELEASE = 1 << 3 | RELEASED,
	};

	struct State {
		int isAction(std::string_view name); // Returns the number of times the action should be triggered since the last update
		bool loadFromFile(std::string_view filepath);

		// Manually manage button state if you want
		void addButton(int button); // Add button which will now be kept track of
		void removeButton(int button); // Remove button from being tracked

		void update(double time); // time used to create repeated actions

		struct Action {
			// Actions can have different ways to activate them, eg. d OR t-ctrl-shift
			// The first key is the trigger for the action
			std::vector<std::vector<int>> combos;
			// Actions can be subsets eg. c vs ctrl-c so when ctrl is held the action shouldn't trigger
			// so don't trigger action if these are held, this should be managed by the class not caller
			std::vector<std::vector<int>> combo_excluders;

			bool continuous = false; // Action is always active while pressed are held, or only when clicked
			bool repeat = false; // Action is triggered every repeat_rate while held
			double repeat_rate = 0.0;
			double last_active = 0.0;
		};

		struct MouseState {
			double last_press = 0.0;
			ButtonState state;
		} left_mouse, right_mouse;

		std::unordered_map<int, ButtonState> button_states; // The buttons we need to keep track of for updating actions
		std::unordered_map<std::string, Action> actions;
		std::unordered_map<std::string, int> action_active;
	};
};

#endif // CONTROLS_CORE_HPP