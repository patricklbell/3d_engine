#version 330 core
in vec3 position;
in vec2 texcoord;
in vec3 tangentDirLightPos; 

uniform sampler2D diffuseMap;
uniform sampler2D normalMap; 
uniform vec3 dirLightColor; 
uniform float time; 

layout (location = 0) out vec3 out_position;
layout (location = 1) out vec3 out_diffuse;
layout (location = 2) out vec3 out_normal;
layout (location = 3) out vec3 out_texcoord;

#define TIME_PERIOD 1000
#define PI 3.14159

void main() {
	// obtain TBN normal from normal map in range [0,1]
    vec3 normal = texture(normalMap, texcoord).rgb;
    // transform normal vector to range [-1,1]
    normal = normalize(normal * 2.0 - 1.0); 

	float lum = max(dot(normal, normalize(tangentDirLightPos)), 0.0);

	out_diffuse = (texture(diffuseMap, texcoord) * vec4((0.3 + 0.7 * lum) * dirLightColor, 1.0)).xyz;
    out_position = position;
    out_normal = normal;
    out_texcoord = vec3(texcoord, 0.0); 
}
