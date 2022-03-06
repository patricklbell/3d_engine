#version 330 core
layout(location = 0) in vec3 in_vertex;
layout(location = 1) in vec2 in_texcoord;
layout(location = 2) in vec3 in_normal;
layout(location = 3) in vec3 in_tangent;

out vec3 position;
out vec2 texcoord;
out vec3 tangentDirLightPos;

uniform mat4 MVP;
uniform mat4 model;
uniform vec3 dirLightPos;

void main(){
	gl_Position = MVP * vec4(in_vertex, 1);
	position = gl_Position.xyz;
	texcoord = in_texcoord;
	vec3 T = normalize(vec3(model * vec4(in_tangent, 0.0)));
	vec3 N = normalize(vec3(model * vec4(-in_normal, 0.0)));
	// re-orthogonalize T with respect to N
	T = normalize(T - dot(T, N) * N);
	// then retrieve perpendicular vector B with the cross product of T and N
	vec3 B = cross(N, T);
	// Transpose of perpendicular matrix is inverse
	mat3 TBN = transpose(mat3(T, B, N));
	tangentDirLightPos = TBN * dirLightPos;
}
