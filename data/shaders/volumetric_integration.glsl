#begin COMPUTE
#macro ANISOTROPY 1
#macro LIGHTS     0

#load lib/constants.glsl

layout(local_size_x = LOCAL_SIZE_X, local_size_y = LOCAL_SIZE_Y, local_size_z = LOCAL_SIZE_Z) in;

layout(binding = 0, rgba16f) uniform writeonly image3D integrated_vol;

#load lib/constants.glsl
#load lib/shared.glsl
#load lib/shadows.glsl
#load lib/utilities.glsl
#if LIGHTS
#load lib/lights_ubo.glsl
#endif

layout(binding = 0) uniform sampler2D bluenoise_map;
layout(binding = 1) uniform sampler3D prev_vol;

uniform mat4 inv_vp; // Reproject from texture coordinates to world coordinates
uniform mat4 prev_vp;
uniform vec3 t1t2e1;

// @todo make fog vols and global height based fog
#if ANISOTROPY
uniform float anisotropy; // How much effect the light direction has on the fogs visibility
#endif
uniform float density;
uniform bool do_accumulation;
uniform ivec3 vol_size;

uniform vec3 wind_direction;
uniform float wind_strength;
uniform float noise_amount;
uniform float noise_scale;

// Noise functions
// https://github.com/Unity-Technologies/VolumetricLighting/blob/master/Assets/VolumetricFog/Shaders/InjectLightingAndDensity.compute
// Used for scrolling fog density
float hash( float n ) { return fract(sin(n)*753.5453123); }
float noisep(vec3 x) {
    vec3 p = floor(x);
    vec3 f = fract(x);
    f = f*f*(3.0-2.0*f);
	
    float n = p.x + p.y*157.0 + 113.0*p.z;
    return mix(mix(mix( hash(n+  0.0), hash(n+  1.0),f.x),
                   mix( hash(n+157.0), hash(n+158.0),f.x),f.y),
               mix(mix( hash(n+113.0), hash(n+114.0),f.x),
                   mix( hash(n+270.0), hash(n+271.0),f.x),f.y),f.z);
}

float ScrollNoise(vec3 pos, float speed, float scale, vec3 dir, float amount, float bias, float mult) {
	float time = time * speed;
	float noiseScale = scale;
	vec3 noiseScroll = dir * time;
	vec3 q = pos - noiseScroll;
	q *= scale;
	float f = 0;
	f = 0.5 * noisep(q);
	// scroll the next octave in the opposite direction to get some morphing instead of just scrolling
	q += noiseScroll * scale;
	q = q * 2.01;
	f += 0.25 * noisep(q);

	f += bias;
	f *= mult;

	f = max(f, 0.0);
	return mix(1.0, f, amount);
}
// -------------------------------------------------------------------------------------------

// Henyey-Greenstein
float phase_function(vec3 V, vec3 light, float g)
{
    float cos_theta = dot(V, light);

    float gsq = g * g;
    float denom  = 1.0 + gsq - 2.0 * g * cos_theta;
    denom = denom * denom * denom;
    denom = sqrt(max(EPSILON, denom));
    return (1.0 - gsq) / denom;
}

float sample_blue_noise(ivec3 coord) {
    ivec2 tex_size = textureSize(bluenoise_map, 0);
    return texelFetch(bluenoise_map, (coord.xy + coord.z) % tex_size, 0).r;
}

vec3 texel_to_world(vec3 texel, ivec3 texture_size) {
    vec3 uvw = (texel + 0.5) / texture_size; // Offset texel and convert to 0 -> 1

    uvw.z = linearToDepth(linearToExponentialDistribution(uvw.z)); // Get better precision by converting to an exponential depth, stil 0 -> 1

    vec3 ndc =  uvw * 2.0 - 1.0; // Convert uv to NDC, -1 -> 1

    // Explains how to determine clipspace from ndc with a conventional projection matrix
    // This might not be necessary but it should reduce some of the error
    // https://www.khronos.org/opengl/wiki/Compute_eye_space_from_window_space
    vec4 clip;
    clip.w = t1t2e1.y / (ndc.z - t1t2e1.x / t1t2e1.z);
    clip.xyz = ndc * clip.w;
    
    vec4 p = inv_vp * clip;
    return p.xyz / p.w;
}

vec3 world_to_uvw(vec3 world, mat4 view_proj) {
    vec4 p = view_proj * vec4(world, 1.0);
    vec3 ndc = p.w > 0.0f ? p.xyz / p.w : p.xyz;

    vec3 uv = (ndc + 1.0) / 2.0;
    uv.z = exponentialToLinearDistribution(depthToLinear(uv.z));

    return uv;
}

float sdBox( vec3 p, vec3 b )
{
  vec3 q = abs(p) - b;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}

vec3 directionalLight(vec3 V, vec3 P) {
    float visibility = 1.0 - shadowness(0.0, P); // @todo @hardcoded good NdtoL to change bias
    vec3 color = visibility * sun_color;
#if ANISOTROPY
    color *= phase_function(V, -sun_direction, anisotropy);
#endif

    return color;
}

#if LIGHTS
vec3 pointLights(vec3 V, vec3 P) {
    vec3 color = vec3(0.0);
    for (int i = 0; i < MAX_LIGHTS; i++) {
        if (any(greaterThan(light_radiances[i], vec3(0)))) {
            vec3 L = light_positions[i] - P;
            float L_sqr = max(dot(L, L), EPSILON);

            vec3 radiance = light_radiances[i] / L_sqr;

#if ANISOTROPY
            color += radiance * phase_function(V, L/sqrt(L_sqr), anisotropy);
#else
            color += radiance;
#endif
        }
    }
    return color;
}
#endif

void main() {
    ivec3 coord = ivec3(gl_GlobalInvocationID.xyz);

    // Check that invocation is inside vol, seems slightly redudant
    if (all(lessThan(coord, vol_size))) {
        // Remap blue noise to -0.5 < j < +0.5
        float jitter = (sample_blue_noise(coord) - 0.5) * (1.0 - EPSILON);

        vec3 jittered_texel = vec3(coord);
        jittered_texel.z += jitter; // Jitter z coordinate, @todo check jittering other components
        vec3 jittered_world_pos = texel_to_world(jittered_texel, vol_size);

        // Get the view direction from the current voxel.
        vec3 V = normalize(camera_position - jittered_world_pos);
        
        vec3 lighting = sun_color*.4; // @todo Set ambient fog color
        lighting += directionalLight(V, jittered_world_pos);
#if LIGHTS
        lighting += pointLights(V, jittered_world_pos);
#endif

        // ScrollNoise(warp, _WindSpeed, _NoiseFogScale, _WindDir, _NoiseFogAmount, -0.3, 8.0);
        float calc_density = density * clamp(ScrollNoise(jittered_world_pos, 3.0*wind_strength, noise_scale, wind_direction, noise_amount, -0.3, 8.0), 0.0, 1.0 - EPSILON);

        // RGB = Amount of in-scattered light, A = Density.
        vec4 color_density = vec4(lighting * calc_density, calc_density);

        // Temporal accumulation
        if (do_accumulation) { 
            vec3 prev_uvw = world_to_uvw(jittered_world_pos, prev_vp);

            if (all(greaterThanEqual(prev_uvw, vec3(0.0))) && all(lessThanEqual(prev_uvw, vec3(1.0)))) {
                vec4 prev_color_density = textureLod(prev_vol, prev_uvw, 0.0);

                // Blend with previous value, probably better techniques (eg motion vectors)
                color_density = mix(prev_color_density, color_density, 0.05f);
            }
        }

        imageStore(integrated_vol, coord, color_density);
    }
}

#end
