#begin VERTEX
#macro ALPHA_CLIP 0
#macro VEGETATION 0
#macro ANIMATED_BONES 0

layout (location = 0) in vec3 in_vertex;

#if ALPHA_CLIP
layout (location = 3) in vec2 in_texcoord;
out VS_OUT {
    vec2 texcoord;
} vs_out;
#endif

#if VEGETATION
layout (location = 1) in vec3 in_normal;
// R - edge stiffness, G - Phase offset, B - overall stiffnes, A - Precomputed ambient occlusion
layout (location = 7) in vec4 in_color;
#endif

#if ANIMATED_BONES
layout (location = 5) in ivec4 in_bone_ids;
layout (location = 6) in vec4  in_weights;
#load lib/bone_ubo.glsl
#endif

#if VEGETATION
uniform float time;
uniform vec2 wind_direction;
uniform float wind_strength;
#load lib/vegetation.glsl
#endif

uniform mat4 model;

void main() {
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
#if ALPHA_CLIP
    vs_out.texcoord = in_texcoord;
#endif

    gl_Position = model * vertex;
}

#end

#begin GEOMETRY
layout(triangles, invocations=CASCADE_NUM) in;
layout(triangle_strip, max_vertices=3) out;

#load lib/shadow_ubo.glsl

#if ALPHA_CLIP
in VS_OUT {
    vec2 texcoord;
} gs_in[];  
out vec2 texcoord;
#endif

void main()
{
    for(int i=0; i < 3; i++){
        gl_Position = shadow_vps[gl_InvocationID] * gl_in[i].gl_Position;
        gl_Layer    = gl_InvocationID;
#if ALPHA_CLIP
        texcoord = gs_in[i].texcoord;
#endif
        EmitVertex();
    }
    EndPrimitive();
}

#end

#begin FRAGMENT

#if ALPHA_CLIP
in vec2 texcoord;

layout (location = 0) uniform sampler2D image;
#endif

void main()
{
#if ALPHA_CLIP
    if(texture(image, texcoord).a < 0.1) {
        discard;
    }
#endif
} 

#end