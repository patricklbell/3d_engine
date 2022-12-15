// hemisphere shader (weighted downsampling of the 3x1 hemisphere layout to a 0.5x0.5 square)
#begin VERTEX

const vec2 ps[4] = vec2[](vec2(1, -1), vec2(1, 1), vec2(-1, -1), vec2(-1, 1));
void main()
{
	gl_Position = vec4(ps[gl_VertexID], 0, 1);
}

#end
#begin FRAGMENT

layout(binding = 0) uniform sampler2D hemispheres;
layout(binding = 1) uniform sampler2D weights;

layout(pixel_center_integer) in vec4 gl_FragCoord; // whole integer values represent pixel centers, GL_ARB_fragment_coord_conventions

out vec4 outColor;

vec4 weightedSample(ivec2 h_uv, ivec2 w_uv, ivec2 quadrant)
{
	vec4 value = texelFetch(hemispheres, h_uv + quadrant, 0);
	vec2 weight = texelFetch(weights, w_uv + quadrant, 0).rg;
	return vec4(value.rgb * weight.r, value.a * weight.g);
}

vec4 threeWeightedSamples(ivec2 h_uv, ivec2 w_uv, ivec2 offset)
{ // horizontal triple sum
	vec4 sum = weightedSample(h_uv, w_uv, offset);
	sum += weightedSample(h_uv, w_uv, offset + ivec2(2, 0));
	sum += weightedSample(h_uv, w_uv, offset + ivec2(4, 0));
	return sum;
}

void main()
{ // this is a weighted sum downsampling pass (alpha component contains the weighted valid sample count)
	vec2 in_uv = gl_FragCoord.xy * vec2(6.0, 2.0) + vec2(0.5);
	ivec2 h_uv = ivec2(in_uv);
	ivec2 w_uv = ivec2(mod(in_uv, vec2(textureSize(weights, 0)))); // there's no integer modulo :(
	vec4 lb = threeWeightedSamples(h_uv, w_uv, ivec2(0, 0));
	vec4 rb = threeWeightedSamples(h_uv, w_uv, ivec2(1, 0));
	vec4 lt = threeWeightedSamples(h_uv, w_uv, ivec2(0, 1));
	vec4 rt = threeWeightedSamples(h_uv, w_uv, ivec2(1, 1));
	outColor = lb + rb + lt + rt;
}

#end