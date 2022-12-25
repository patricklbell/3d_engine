#load lib/shared.glsl
layout(binding = 10) uniform sampler3D accumulated_volumetric_vol;

// Should probably be in lib
// https://gist.github.com/Fewes/59d2c831672040452aa77da6eaab2234
vec4 textureTricubic(sampler3D tex, vec3 coord)
{
	// Shift the coordinate from [0,1] to [-0.5, texture_size-0.5]
  vec3 texture_size = vec3(textureSize(tex, 0));
	vec3 coord_grid = coord * texture_size - 0.5;
	vec3 index = floor(coord_grid);
	vec3 fraction = coord_grid - index;
	vec3 one_frac = 1.0 - fraction;

	vec3 w0 = 1.0/6.0 * one_frac*one_frac*one_frac;
	vec3 w1 = 2.0/3.0 - 0.5 * fraction*fraction*(2.0-fraction);
	vec3 w2 = 2.0/3.0 - 0.5 * one_frac*one_frac*(2.0-one_frac);
	vec3 w3 = 1.0/6.0 * fraction*fraction*fraction;

	vec3 g0 = w0 + w1;
	vec3 g1 = w2 + w3;
	vec3 mult = 1.0 / texture_size;
	vec3 h0 = mult * ((w1 / g0) - 0.5 + index); //h0 = w1/g0 - 1, move from [-0.5, texture_size-0.5] to [0,1]
	vec3 h1 = mult * ((w3 / g1) + 1.5 + index); //h1 = w3/g1 + 1, move from [-0.5, texture_size-0.5] to [0,1]

	// Fetch the eight linear interpolations
	// Weighting and fetching is interleaved for performance and stability reasons
	vec4 tex000 = texture(tex, h0);
	vec4 tex100 = texture(tex, vec3(h1.x, h0.y, h0.z));
	tex000 = mix(tex100, tex000, g0.x); // Weigh along the x-direction

	vec4 tex010 = texture(tex, vec3(h0.x, h1.y, h0.z));
	vec4 tex110 = texture(tex, vec3(h1.x, h1.y, h0.z));
	tex010 = mix(tex110, tex010, g0.x); // Weigh along the x-direction
	tex000 = mix(tex010, tex000, g0.y); // Weigh along the y-direction

	vec4 tex001 = texture(tex, vec3(h0.x, h0.y, h1.z));
	vec4 tex101 = texture(tex, vec3(h1.x, h0.y, h1.z));
	tex001 = mix(tex101, tex001, g0.x); // Weigh along the x-direction

	vec4 tex011 = texture(tex, vec3(h0.x, h1.y, h1.z));
	vec4 tex111 = texture(tex, vec3(h1));
	tex011 = mix(tex111, tex011, g0.x); // Weigh along the x-direction
	tex001 = mix(tex011, tex001, g0.y); // Weigh along the y-direction

	return mix(tex001, tex000, g0.z); // Weigh along the z-direction
}

#macro TRICUBIC 1

#load lib/constants.glsl
#load lib/utilities.glsl

vec3 addInscatteredVolumetrics(vec3 color, vec3 frag_coord) {
	float uv_z = exponentialToLinearDistribution(depthToLinear(frag_coord.z));
	vec3 uv = vec3(frag_coord.xy / (window_size - 1.0), uv_z);

#if TRICUBIC
	vec4 scattered_light = textureTricubic(accumulated_volumetric_vol, uv);
#else
	vec4 scattered_light = texture(accumulated_volumetric_vol, uv);
#endif
	float transmittance = scattered_light.a;

	return color * transmittance + scattered_light.rgb; // @todo pbr energy
}
