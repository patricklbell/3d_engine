#begin VERTEX
layout (location = 0) in vec3 in_position;

uniform mat4 untranslated_view_projection;

out vec3 texcoords;

void main()
{
    texcoords = in_position;
    gl_Position = untranslated_view_projection * vec4(in_position, 1.0);
    // Makes depth infinite @optimize? because part of multiplication is not needed
    gl_Position.z = gl_Position.w;
}
#end
#begin FRAGMENT

#macro VOLUMETRICS 1

in vec3 texcoords;

layout (location = 0) out vec4 out_color;

layout(binding = 3) uniform samplerCube skybox;

#if VOLUMETRICS
#load lib/volumetrics.glsl
#endif

void main()
{
    out_color = texture(skybox, texcoords);
#if VOLUMETRICS
    out_color.rgb = addInscatteredVolumetrics(out_color.rgb, gl_FragCoord.xyz);
#endif
}

#end