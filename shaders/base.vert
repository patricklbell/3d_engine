#version 330 core
layout(location = 0) in vec3 in_vertex;
layout(location = 1) in vec2 in_texcoord;
layout(location = 2) in vec3 in_normal;

uniform mat4 MVP;

out vec3 normal;
out vec3 position;
out vec2 texcoord;

void main(){
	gl_Position = MVP * vec4(in_vertex, 1);
	position = gl_Position.xyz;
	normal = normalize(mat3(MVP) * in_normal);
	texcoord = in_texcoord;
}
