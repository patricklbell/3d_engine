#begin VERTEX
#macro WATER            0
#macro VEGETATION       0
#macro ANIMATED_BONES   0

layout (location = 0) in vec3 in_vertex;
layout (location = 1) in vec3 in_normal;

#if ANIMATED_BONES
layout (location = 5) in ivec4 in_bone_ids;
layout (location = 6) in vec4  in_weights;
#load lib/bone_ubo.glsl
#endif

#if WATER
layout (location = 3) in vec2 in_texcoord;
#load lib/waves.glsl
#endif

#if VEGETATION
// R - edge stiffness, G - Phase offset, B - overall stiffnes, A - Precomputed ambient occlusion
layout (location = 7) in vec4 in_color;
#endif

uniform mat4 mvp;
uniform mat4 model;
uniform float time;

out VS_FS {
  vec3 normal;
} vs_out;

void main()
{
#if ANIMATED_BONES
    mat4 bone_transform = mat4(0.0);
    for(int i = 0 ; i < MAX_BONE_WEIGHTS; i++) {
        if(in_bone_ids[i] == -1) 
            continue;

        bone_transform += bone_matrices[in_bone_ids[i]] * in_weights[i];
    }

    vec4 vertex = bone_transform * vec4(in_vertex, 1.0);
#else
    vec4 vertex = vec4(in_vertex, 1.0);
#endif

#if VEGETATION
    vertex.xyz = applyWindToVegetation(vertex.xyz, in_normal, in_color.r, in_color.g, in_color.b, wind_direction, wind_strength, time);
#endif

#if WATER
    vec3 tangent, binormal, normal;
    waves(vertex.xyz, in_texcoord, time, vertex.xyz, normal, tangent, binormal);

    vs_out.normal = (model*vec4(normal, 0.0)).xyz;
#else
    vs_out.normal = (model*vec4(in_normal, 0.0)).xyz;
#endif

    gl_Position = mvp * vertex;
}
#end

#begin FRAGMENT

in VS_FS {
  vec3 normal;
} fs_in;

out vec4 out_color;

uniform vec3 sun_direction;
uniform vec4 color;
uniform vec4 color_flash_to;
uniform float flashing;
uniform float shaded;
uniform float time;

#define PI                 3.14159265359f

void main()
{
    vec4 color1 = color;
    if(flashing==1.0f){
        color1 = mix(color, color_flash_to, (1.0f+sin(2.0f*PI*time - PI * 0.5f)) / 2.0f);
    }
    if(shaded==1.0f){
        out_color = vec4((0.3 + max(0.7*dot(fs_in.normal, -sun_direction), 0))*color1.rgb, color1.a);
    } else {
        out_color = color1;
    }
}

#end