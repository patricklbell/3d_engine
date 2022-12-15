#include <iostream>
#include <fstream>
#include <sstream>

#include <shader/core.hpp>
#include <shader/globals.hpp>

#include <utilities/strings.hpp>

static uint64_t compute_bitmap_hash(std::unordered_map<std::string, bool>& macros);

Shader::~Shader() {
	for (auto& p : programs) {
		glDeleteProgram(p.second);
	}
}

bool Shader::ready() {
	return active_program != GL_FALSE;
}

GLuint Shader::uniform(std::string_view name) {
	auto lu = programs_uniforms[active_hash].find(std::string(name));
	if (lu == programs_uniforms[active_hash].end()) {
		std::cerr << "Unknown uniform " << name << " for shader " << handle << "\n";
		return GL_FALSE;
	}
	else {
		return lu->second;
	}
}

bool Shader::update_from_dependencies() {
	for (auto& p : dependencies) {
		const auto& sd = p.second;
		if (std::filesystem::exists(sd.path) && sd.last_write_time != std::filesystem::last_write_time(sd.path)) {
			clear_loaded();
			macros.clear();
			programs.clear();
			programs_uniforms.clear();
			if (!load_file(handle)) return false; // First file is core
			return compile();
		}
	}
	return true;
}

static bool load_shader_chunk(std::ifstream &f, std::string &chunk, 
	std::unordered_map<std::string, Shader::FileDependency> &dependencies,
	std::unordered_map<std::string, bool> &macros) {
	std::string line;
	while (f) {
		std::getline(f, line);
		if (!f) break;

		constexpr std::string_view load_trigger = "#load ";
		constexpr std::string_view macro_trigger = "#macro ";
		if (startsWith(line, load_trigger)) {
			if (line.size() > load_trigger.size()) {
				// @todo make this handle any relative path
				std::string loadpath = "data/shaders/" + line.substr(load_trigger.size());
				if (!std::filesystem::exists(loadpath)) {
					std::cerr << "Invalid #load in shader, " << loadpath << " doesn't exists\n";
					continue;
				}

				if (dependencies.find(loadpath) == dependencies.end()) {
					dependencies[loadpath].path = loadpath;
					dependencies[loadpath].last_write_time = std::filesystem::last_write_time(loadpath);

					load_shader_chunk(std::ifstream(loadpath), chunk, dependencies, macros);
				}
			}
		}
		else if (startsWith(line, macro_trigger)) {
			if (line.size() > load_trigger.size()) {
				// @todo make this handle any relative path
				std::istringstream ss(line.substr(load_trigger.size()));
				std::string name;
				ss >> name;
				bool value;
				ss >> value;
				macros[name] = value;
			}
		}
		else if (line == "#end") {
			return true;
		}
		else {
			chunk += line + '\n';
		}
	}
	return false;
}

bool Shader::load_file(std::string_view path) {
	std::ifstream f(path);
	if (!f) {
		std::cerr << "Failed to open shader file " << path << "\n";
		return false;
	}

	handle = path;
	dependencies[handle].path = handle;
	dependencies[handle].last_write_time = std::filesystem::last_write_time(path);

	std::cout << "Loading shader " << handle << "\n";

	std::string line;
	Shader::Type stage_type = Shader::Type::NONE;
	while (f) {
		std::getline(f, line);
		if (!f) break;

		if (line == "#begin VERTEX") {
			stage_type = Shader::Type::VERTEX;
		}
		else if (line == "#begin FRAGMENT") {
			stage_type = Shader::Type::FRAGMENT;
		}
		else if (line == "#begin GEOMETRY") {
			stage_type = Shader::Type::GEOMETRY;
		}
		else if (line == "#begin COMPUTE") {
			stage_type = Shader::Type::COMPUTE;
		}
		else {	
			continue;
		}

		if (!type_to_chunk[stage_type].size()) {
			auto& chunk = type_to_chunk[stage_type];

			std::unordered_map<std::string, FileDependency> stage_dependencies;
			if (!load_shader_chunk(f, chunk, stage_dependencies, macros)) 
				return false;
			dependencies.merge(stage_dependencies);
		}
		else {
			std::cerr << "Duplicate section: " << line << " encountered loading " << path << ", breaking\n";
			break;
		}
	}

	file_loaded = true;
	return true;
}

bool Shader::load_file_compile(std::string_view path, std::string_view _prepend) {
	if (!load_file(path)) return false;
	return compile(_prepend);
}

void Shader::clear_loaded() {
	file_loaded = false;
	dependencies.clear();
	type_to_chunk.clear();
}

static bool check_shader_errors(GLuint id, bool program = false) {
	int info_log_length;
	program ? glGetProgramiv(id, GL_INFO_LOG_LENGTH, &info_log_length) : glGetShaderiv(id, GL_INFO_LOG_LENGTH, &info_log_length);
	if (info_log_length > 0) {
		char* info_log = (char*)malloc(info_log_length + 1);
		glGetShaderInfoLog(id, info_log_length, NULL, info_log);
		std::cerr << "Error in shader:\n" << info_log << "\n";
		free(info_log);

		return true;
	}
	return false;
}

static void print_shader_sources(std::vector<char*> &sources) {
	int line_i = 1;
	std::string line;
	for (const auto& src : sources) {
		std::istringstream ss(src);
		while (ss) {
			std::getline(ss, line);
			if (!ss) break;

			std::cout << std::setfill(' ') << std::setw(3) << line_i << ". " << line << "\n";
			line_i++;
		}
	}
}

static GLuint compile_shader(GLenum shader_type, std::vector<char*> sources) {
	GLuint id = glCreateShader(shader_type);
	glShaderSource(id, sources.size(), &sources[0], NULL);
	glCompileShader(id);

	if (check_shader_errors(id)) {
		return GL_FALSE;
	}
	return id;
}

static void get_uniforms_from_program(GLuint program_id, std::unordered_map<std::string, GLuint> &uniforms) {
	GLint count;
	glGetProgramiv(program_id, GL_ACTIVE_UNIFORMS, &count);
	//printf("Active Uniforms: %d\n", count);

	const GLsizei buf_size = 256; // maximum name length
	//glGetProgramiv(shader.program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &buf_size);
	GLchar name[buf_size]; // variable name in GLSL
	GLsizei length; // name length

	GLint size; // size of the variable
	GLenum type; // type of the variable (float, vec3 or mat4, etc)
	for (GLint i = 0; i < count; i++) {
		glGetActiveUniform(program_id, (GLuint)i, buf_size, &length, &size, &type, name);
		uniforms[name] = glGetUniformLocation(program_id, name);

		//printf("Uniform #%d Type: %u Name: %s\n", i, type, name);
	}
}

static const std::unordered_map<Shader::Type, std::string> shader_type_to_name = {
	{ Shader::Type::VERTEX, "Vertex" },
	{ Shader::Type::FRAGMENT, "Fragment" },
	{ Shader::Type::GEOMETRY, "Geometry" },
	{ Shader::Type::COMPUTE, "Compute" },
};

// Note that prepend should be null terminated
bool Shader::compile(std::string_view _prepend) {
	std::cout << "Compiling shader " << handle << "\n";
	if (_prepend != "") 
		prepend = _prepend;

	std::string macros_str = "";
	for (const auto& p : macros) {
		if (p.second) {
			macros_str += "#define " + p.first + " " + std::to_string(p.second) + "\n";
		}
	}
	std::vector<char*> sources = { (char*)prepend.data(), (char*)macros_str.c_str(), nullptr };
	std::vector<GLuint> shader_ids;

	for (const auto& p : type_to_chunk) {
		const auto& type = p.first;
		const auto& chunk = p.second;
		sources[sources.size() - 1] = (char*)chunk.c_str();

		GLuint id = GL_FALSE;
		switch (type)
		{
		case Type::VERTEX:
			id = compile_shader(GL_VERTEX_SHADER, sources);
			break;
		case Type::FRAGMENT:
			id = compile_shader(GL_FRAGMENT_SHADER, sources);
			break;
		case Type::GEOMETRY:
			id = compile_shader(GL_GEOMETRY_SHADER, sources);
			break;
		case Type::COMPUTE:
			id = compile_shader(GL_COMPUTE_SHADER, sources);
			break;
		default:
			break;
		}

		if (id == GL_FALSE) {
			for (const auto& id : shader_ids) {
				glDeleteShader(id);
			}
			std::cerr << "Failed to compile sub-shader " << shader_type_to_name.find(type)->second << " for file " << handle << ", preprocessor produced:\n";
			print_shader_sources(sources);
			return false;
		}
		shader_ids.push_back(id);
	}

	GLuint program_id = glCreateProgram();
	for (const auto& id : shader_ids) {
		glAttachShader(program_id, id);
	}
	glLinkProgram(program_id);

	if (check_shader_errors(program_id, true)) {
		std::cerr << "Failed to link shader program, preprocessor produced:\n";
		print_shader_sources(sources);
		return false;
	}

	for (const auto& id : shader_ids) {
		glDetachShader(program_id, id);
		glDeleteShader(id);
	}

	const auto hash = compute_bitmap_hash(macros);
	get_uniforms_from_program(program_id, programs_uniforms[hash]);
	programs[hash] = program_id;
	active_hash = hash;
	active_program = program_id;

	return true;
}

bool Shader::activate_macros() {
	auto hash = compute_bitmap_hash(macros);
	if (active_hash == hash) 
		return false;

	auto& program_lu = programs.find(hash);
	if (program_lu == programs.end()) {
		compile();
		return true;
	}

	active_hash = hash;
	active_program = program_lu->second;

	return false;
}

bool Shader::set_macro(std::string_view macro, bool value, bool activate) {
	auto& macro_lu = macros.find(std::string(macro));
	if (macro_lu == macros.end()) {
		std::cerr << "Unknown macro " << macro << " for shader " << handle << "\n";
		return false;
	}
	if (value == macro_lu->second) return true;

	macro_lu->second = value;

	if (activate) {
		activate_macros();
	}
}

// ----------------------------------------------------------------------------

static uint64_t compute_bitmap_hash(std::unordered_map<std::string, bool>& macros) {
	uint64_t h = 0;
	int i = 0;
	for (const auto& p : macros) {
		if (p.second) {
			h |= 1ULL << i;
		}
		i++;
	}

	return h;
}