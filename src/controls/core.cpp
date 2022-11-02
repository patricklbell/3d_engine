#include <sstream>
#include <string>
#include <fstream>
#include <set>
#include <iostream>

#include <controls/core.hpp>
#include <controls/globals.hpp>
#include <globals.hpp>

using namespace Controls;

static ButtonState getGlfwButtonState(int button) {
	int glfw_state = glfwGetKey(window, button);
	switch (glfw_state)
	{
	case GLFW_PRESS:
		return HELD;
	default:
		return RELEASED;
	}
}

void State::addButton(int button) {
	// This intial state can't determine if the button has just been pressed
	button_states.emplace(button, getGlfwButtonState(button)); 
}

static void getButtonsFromActions(State& state) {
	for (const auto& p : state.actions) {
		const auto& a = p.second;

		for (const auto& buttons : a.combos) {
			for (const auto& b : buttons) {
				if (state.button_states.find(b) == state.button_states.end()) {
					state.addButton(b);
				}
			}
		}
		for (const auto& buttons : a.combo_excluders) {
			for (const auto& b : buttons) {
				if (state.button_states.find(b) == state.button_states.end()) {
					state.addButton(b);
				}
			}
		}
	}
}

void State::removeButton(int button) {
	button_states.erase(button);
	getButtonsFromActions(*this);
}

void State::update(double time) {
	// Update all the button states from glfw
	for (auto& p : button_states) {
		const auto& b = p.first;
		auto& state = p.second;

		auto new_state = getGlfwButtonState(b);
		if ((state & RELEASED) && (new_state & HELD)) {
			state = PRESS;
		}
		else if ((state & HELD) && (new_state & RELEASED)) {
			state = RELEASE;
		}
		else {
			state = new_state;
		}
	}

	// Update action states based on new button states
	for (auto& p : actions) {
		auto& a = p.second;
		const auto& name = p.first;

		// whether a repeating action has just been triggered
		bool repeat_first = false; 

		bool active = false;
		for(int i = 0; i < a.combos.size(); ++i) {
			const auto& buttons = a.combos[i];

			bool combo_active = true;
			for (const auto& b : buttons) {
				// Action is active while all keys in combo are held
				if (!(button_states[b] & HELD)) {
					combo_active = false;
					break;
				}
			}
			if (!combo_active)
				continue;
			
			repeat_first = false;
			// Non continuous actions are only active when the trigger button has just been pressed
			if (!a.continuous && !(button_states[buttons[0]] == PRESS)) {
				if (a.repeat) {
					a.last_active = time;
					repeat_first = true;
				}
				else {
					continue;
				}
			}

			active = true;

			// Check that none of the excluders are held
			const auto& excluders = a.combo_excluders[i];
			for (const auto& b : excluders) {
				if (button_states[b] & HELD) {
					active = false;
					break;
				}
			}

			if(active)
				break;
		}

		if (active && a.repeat && !repeat_first && !a.continuous) {
			int action_num = glm::floor((time - a.last_active) / a.repeat_rate);
			action_active[name] = action_num;
			if (action_num) {
				a.last_active = time;
			}
		}
		else {
			action_active[name] = active;
		}
	}

	// Update mouse state
	auto new_left_mouse = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
	if (new_left_mouse == GLFW_PRESS && (left_mouse.state & RELEASED)) {
		left_mouse.state = PRESS;
		left_mouse.last_press = time;
	}
	else if (new_left_mouse == GLFW_RELEASE && (left_mouse.state & HELD)) {
		left_mouse.state = RELEASE;
	}
	else {
		if (new_left_mouse == GLFW_PRESS)
			left_mouse.state = HELD;
		else
			left_mouse.state = RELEASED;
	}
	auto new_right_mouse = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
	if (new_right_mouse == GLFW_PRESS && (right_mouse.state & RELEASED)) {
		right_mouse.state = PRESS;
		right_mouse.last_press = time;
	}
	else if (new_right_mouse == GLFW_RELEASE && (right_mouse.state & HELD)) {
		right_mouse.state = RELEASE;
	}
	else {
		if (new_right_mouse == GLFW_PRESS)
			right_mouse.state = HELD;
		else
			right_mouse.state = RELEASED;
	}
}

int State::isAction(std::string_view name) {
	const auto& lu = action_active.find(name.data());
	if (lu != action_active.end()) {
		return lu->second;
	}

	std::cerr << "Unknown action: " << name << "\n";
	return 0;
}

// --------------------------------Keycode LUT-------------------------------- //
static void print_keyname_code_map() {
	for (int i = 0; i <= GLFW_KEY_LAST; ++i) {
		auto name = glfwGetKeyName(i, 0);
		if (name != NULL)
			std::cout << "{\"" << name << "\", " << i << "},\n";
	}
}
// produces this lut on windows
const std::unordered_map<std::string, int> keyname_to_code = {
	{"'", 39},
	{",", 44},
	{"-", 45},
	{".", 46},
	{"/", 47},
	{"0", 48},
	{"1", 49},
	{"2", 50},
	{"3", 51},
	{"4", 52},
	{"5", 53},
	{"6", 54},
	{"7", 55},
	{"8", 56},
	{"9", 57},
	{";", 59},
	{"=", 61},
	{"a", 65},
	{"b", 66},
	{"c", 67},
	{"d", 68},
	{"e", 69},
	{"f", 70},
	{"g", 71},
	{"h", 72},
	{"i", 73},
	{"j", 74},
	{"k", 75},
	{"l", 76},
	{"m", 77},
	{"n", 78},
	{"o", 79},
	{"p", 80},
	{"q", 81},
	{"r", 82},
	{"s", 83},
	{"t", 84},
	{"u", 85},
	{"v", 86},
	{"w", 87},
	{"x", 88},
	{"y", 89},
	{"z", 90},
	{"[", 91},
	{"\\", 92},
	{"]", 93},
	{"`", 96},
	{"0", 320},
	{"1", 321},
	{"2", 322},
	{"3", 323},
	{"4", 324},
	{"5", 325},
	{"6", 326},
	{"7", 327},
	{"8", 328},
	{"9", 329},
	{".", 330},
	{"/", 331},
	{"*", 332},
	{"-", 333},
	{"+", 334},
	{"space", GLFW_KEY_SPACE},
	/* Function keys */
	{"escape", 256},
	{"enter", 257},
	{"tab", 258},
	{"backspace", 259},
	{"insert", 260},
	{"delete", 261},
	{"right", 262},
	{"left", 263},
	{"down", 264},
	{"up", 265},
	{"page_up", 266},
	{"page_down", 267},
	{"home", 268},
	{"end", 269},
	{"caps_lock", 280},
	{"scroll_lock", 281},
	{"num_lock", 282},
	{"print_screen", 283},
	{"pause", 284},
	{"f1", 290},
	{"f2", 291},
	{"f3", 292},
	{"f4", 293},
	{"f5", 294},
	{"f6", 295},
	{"f7", 296},
	{"f8", 297},
	{"f9", 298},
	{"f10", 299},
	{"f11", 300},
	{"f12", 301},
	{"f13", 302},
	{"f14", 303},
	{"f15", 304},
	{"f16", 305},
	{"f17", 306},
	{"f18", 307},
	{"f19", 308},
	{"f20", 309},
	{"f21", 310},
	{"f22", 311},
	{"f23", 312},
	{"f24", 313},
	{"f25", 314},
	{"kp_0", 320},
	{"kp_1", 321},
	{"kp_2", 322},
	{"kp_3", 323},
	{"kp_4", 324},
	{"kp_5", 325},
	{"kp_6", 326},
	{"kp_7", 327},
	{"kp_8", 328},
	{"kp_9", 329},
	{"kp_decimal", 330},
	{"kp_divide", 331},
	{"kp_multiply", 332},
	{"kp_subtract", 333},
	{"kp_add", 334},
	{"kp_enter", 335},
	{"kp_equal", 336},
	{"left_shift", 340},
	{"left_control", 341},
	{"left_alt", 342},
	{"left_super", 343},
	{"right_shift", 344},
	{"right_control", 345},
	{"right_alt", 346},
	{"right_super", 347},
	{"menu", 348},
};
// --------------------------------------------------------------------------- //

bool State::loadFromFile(std::string_view filepath) {
	std::ifstream infile(filepath.data());
	if (!infile) {
		return false;
	}
	std::cout << "Loading actions " << filepath << ".\n";

	std::string line;
	while (std::getline(infile, line)) {
		std::istringstream iss(line);

		// Skip comments and empty lines
		if (line.size() < 2 || (line[0] == '/' && line[1] == '/')) {
			continue;
		}

		int is_continuous;
		std::string action_name, trigger_key_name;
		if (!(iss >> action_name >> is_continuous >> trigger_key_name)) { 
			std::cerr << "Bad line in action file: " << line;
			continue;
		}

		std::vector<std::string> keynames;
		keynames.push_back(trigger_key_name);

		std::string keyname;
		while (iss >> keyname) {
			keynames.push_back(keyname);
		}

		bool keynames_valid = true;
		std::vector<int> buttons;
		for (const auto& keyname : keynames) {
			const auto& lu = keyname_to_code.find(keyname);
			if (lu != keyname_to_code.end()) {
				buttons.push_back(lu->second);
			}
			else {
				std::cerr << "Unkown keyname: " << keyname << " in action " << action_name << ", skipping\n";
				keynames_valid = false;
				break;
			}
		}

		if (!keynames_valid)
			continue;

		auto& a = actions[action_name];
		action_active[action_name] = false;
		a.combos.emplace_back(std::move(buttons));
		a.combo_excluders.emplace_back(std::vector<int>{});
		a.continuous = is_continuous;

		//@debug
		/*std::cout << "Action " << action_name << ": ";
		bool first = true;
		for (const auto& k: keynames) {
			if (!first)
				std::cout << "+";
			std::cout << k;
			first = false;
		}
		std::cout << "\n";*/
		// @todo other settings
	}

	// Determine exclusion keys by considering collisions between key sets
	std::vector<std::set<int>> keysets;
	std::vector<std::pair<std::string, int>> keyset_infos; // action name and combo index
	for(auto& p: actions) {
		auto& name = p.first;
		auto& a = p.second;

		for (int k = 0; k < a.combos.size(); ++k) {
			auto& buttons = a.combos[k];
			std::set<int> keyset(buttons.begin(), buttons.end());
			
			for (int j = 0; j < keysets.size(); ++j) {
				const auto& existing_keyset = keysets[j];
				const auto& existing_info = keyset_infos[j];

				std::set<int> intersection;
				std::set_intersection(existing_keyset.begin(), existing_keyset.end(),
									  keyset.begin(), keyset.end(),
									  std::inserter(intersection, intersection.begin()));

				// Check if keyset is a subset of an existing keyset => add the difference as excluders
				if (keyset.size() < existing_keyset.size() && intersection.size() == keyset.size()) {
					auto& excluder = a.combo_excluders[k];

					std::set<int> difference;
					std::set_difference(existing_keyset.begin(), existing_keyset.end(),
										keyset.begin(), keyset.end(),
										std::inserter(difference, difference.begin()));
					for (const auto& b : difference) {
						excluder.push_back(b);
					}
				}
				// Check if keyset is a superset of an existing keyset => add the difference as excluders to the subset
				else if (existing_keyset.size() < keyset.size() && intersection.size() == existing_keyset.size()) {
					auto& excluder = actions[existing_info.first].combo_excluders[existing_info.second];

					std::set<int> difference;
					std::set_difference(keyset.begin(), keyset.end(),
										existing_keyset.begin(), existing_keyset.end(),
										std::inserter(difference, difference.begin()));
					for (const auto& b : difference) {
						excluder.push_back(b);
					}
				}
			}
			
			keysets.emplace_back(std::move(keyset));
			keyset_infos.emplace_back(std::pair{ name, k });
		}
	}

	// Get rid of duplicate buttons in excluders
	for (auto& p : actions) {
		for (auto& e : p.second.combo_excluders) {
			std::set<int> s;

			for (auto iter = e.begin(); iter != e.end(); ) {
				if (s.find(*iter) == s.end()) {
					s.insert(*iter);
					iter++;
				}
				else {
					iter = e.erase(iter);
				}
			}
		}
	}

	return true;
}