#begin VERTEX

const vec2 ps[4] = vec2[](vec2(1, -1), vec2(1, 1), vec2(-1, -1), vec2(-1, 1));
void main()
{
	gl_Position = vec4(ps[gl_VertexID], 0, 1);
};

#end
#begin FRAGMENT

layout(binding = 0) uniform sampler2D hemispheres;

layout(pixel_center_integer) in vec4 gl_FragCoord; // whole integer values represent pixel centers, GL_ARB_fragment_coord_conventions

out vec4 outColor;

void main()
{	// this is a sum downsampling pass (alpha component contains the weighted valid sample count)
	ivec2 h_uv = ivec2(gl_FragCoord.xy) * 2;
	vec4 lb = texelFetch(hemispheres, h_uv + ivec2(0, 0), 0);
	vec4 rb = texelFetch(hemispheres, h_uv + ivec2(1, 0), 0);
	vec4 lt = texelFetch(hemispheres, h_uv + ivec2(0, 1), 0);
	vec4 rt = texelFetch(hemispheres, h_uv + ivec2(1, 1), 0);
	outColor = lb + rb + lt + rt;
};

#end