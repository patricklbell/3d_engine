layout(binding = 5) uniform sampler2DArray shadow_map;
layout(binding = 6) uniform sampler3D jitter_vol; // Only needed if using Nvidia method

#load lib/shadow_ubo.glsl

//const vec2 poisson_disk[16] = vec2[]( 
//   vec2( -0.94201624, -0.39906216 ), 
//   vec2( 0.94558609, -0.76890725 ), 
//   vec2( -0.094184101, -0.92938870 ), 
//   vec2( 0.34495938, 0.29387760 ), 
//   vec2( -0.91588581, 0.45771432 ), 
//   vec2( -0.81544232, -0.87912464 ), 
//   vec2( -0.38277543, 0.27676845 ), 
//   vec2( 0.97484398, 0.75648379 ), 
//   vec2( 0.44323325, -0.97511554 ), 
//   vec2( 0.53742981, -0.47373420 ), 
//   vec2( -0.26496911, -0.41893023 ), 
//   vec2( 0.79197514, 0.19090188 ), 
//   vec2( -0.24188840, 0.99706507 ), 
//   vec2( -0.81409955, 0.91437590 ), 
//   vec2( 0.19984126, 0.78641367 ), 
//   vec2( 0.14383161, -0.14100790 ) 
//);
//
//// Returns a random number based on a vec3 and an int.
//float random(vec3 seed, int i){
//	vec4 seed4 = vec4(seed,i);
//	float dot_product = dot(seed4, vec4(12.9898,78.233,45.164,94.673));
//	return fract(sin(dot_product) * 43758.5453);
//}

#define SMOOTH_SHADOWS 0

// Smaller intial number of samples which determine whether we are on a boundary
#define SAMPLES_COUNT_1 4
#define INV_SAMPLES_COUNT_1 (1.0f / float(SAMPLES_COUNT_1))    
#define SAMPLES_COUNT 16
#define INV_SAMPLES_COUNT (1.0f / float(SAMPLES_COUNT))    
#define SHADOW_FILTER_SIZE 50.5
#define JITTER_SIZE 10.5f

float calculateShadowness(float NdotL, vec3 position, int layer) {
    vec4 shadow_pos = shadow_vps[layer] * vec4(position, 1.0);
    // perform perspective divide
    vec3 shadow_coord = shadow_pos.xyz / shadow_pos.w;
    // transform to [0,1] range
    shadow_coord = shadow_coord * 0.5 + 0.5;

    // get depth of current fragment from light's perspective
    float shadow_depth = shadow_coord.z;

    // return unshadowed result when point is beyond closer than near_plane of the light's frustum.
    if (shadow_depth > 1.0)
    {
        return 0.0;
    }

    // calculate bias (based on depth map resolution and slope)
    float bias = max(0.002 * (1.0 - NdotL), 0.005);
    if (layer == CASCADE_NUM)
    {
        bias *= 1 / (far_plane * 0.5f);
    }
    else
    {
        bias *= 1 / (shadow_cascade_distances[layer] * 0.5f);
    }
    // PCF
    vec2 texel_size = 1.0 / vec2(textureSize(shadow_map, 0));

    float shadow = 0.0f;
    
#if SMOOTH_SHADOWS
    // Nvidia method for softer penumbra result from:
    // https://developer.nvidia.com/gpugems/gpugems2/part-ii-shading-lighting-and-shadows/chapter-17-efficient-soft-edged-shadows-using
#if COMPILING_FS
    vec3 jcoord = vec3(gl_FragCoord.xy * JITTER_SIZE, 0); // Take screen space position to vary result
    vec2 fsize = far_plane * bias * SHADOW_FILTER_SIZE * texel_size; // Size of pcf filter, @todo adjust for csm (further away -> lower resolution)
#else // This obvious this won't work but need a fake value to compile
    vec3 jcoord = vec3(0);
    vec2 fsize = SHADOW_FILTER_SIZE * texel_size;
#endif
    
    shadow_depth -= bias;

    // Initially sample edges of disk to see if we are at border of a shadow
    for (int i = 0; i < SAMPLES_COUNT_1; i++) {
        vec4 offset = texture(jitter_vol, jcoord);
        jcoord.z += INV_SAMPLES_COUNT;

        // Jitters are stored doubly in rg and ba channels
        float pcf_depth = texture(shadow_map, vec3(shadow_coord.xy + offset.xy * fsize, layer)).r; 
        shadow += shadow_depth > pcf_depth ? INV_SAMPLES_COUNT_1 : 0.0;

        pcf_depth = texture(shadow_map, vec3(shadow_coord.xy + offset.zw * fsize, layer)).r; 
        shadow += shadow_depth > pcf_depth ? INV_SAMPLES_COUNT_1 : 0.0;
    }

    // product is zero, we skip expensive shadow-map filtering  
    if (shadow > 0.0 && shadow < 1.0) {    // most likely, we are in the penumbra      
        shadow *= SAMPLES_COUNT_1 * INV_SAMPLES_COUNT; // adjust running total      
        // refine our shadow estimate    
        for (int i = 0; i < SAMPLES_COUNT - SAMPLES_COUNT_1; i++) {
            vec4 offset = texture(jitter_vol, jcoord);
            jcoord.z += INV_SAMPLES_COUNT;

            // Jitters are stored doubly in rg and ba channels
            float pcf_depth = texture(shadow_map, vec3(shadow_coord.xy + offset.xy * fsize, layer)).r; 
            shadow += shadow_depth > pcf_depth ? INV_SAMPLES_COUNT : 0.0;

            pcf_depth = texture(shadow_map, vec3(shadow_coord.xy + offset.zw * fsize, layer)).r; 
            shadow += shadow_depth > pcf_depth ? INV_SAMPLES_COUNT : 0.0;
        }
    }
    shadow *= 0.5;
#else // !SMOOTH_SHADOWS
    // Cheaper method based on, (still PCF)
    // https://learn.microsoft.com/en-us/windows/win32/dxtecharts/cascaded-shadow-maps and
    // https://developer.nvidia.com/sites/all/modules/custom/gpugems/books/GPUGems/gpugems_ch11.html
    for(float x = -1.5; x <= 1.5; x+=1.0)
    {
        for(float y = -1.5; y <= 1.5; y+=1.0)
        {
            float pcf_depth = texture(shadow_map, vec3(shadow_coord.xy + vec2(x, y) * texel_size, layer)).r; 
            shadow += (shadow_depth - bias) > pcf_depth ? 1.0 : 0.0;
        }    
    }
    shadow /= 16.0f;
#endif // SMOOTH_SHADOWS

    return shadow;
}

#define CSM_BLEND_BAND 0.5

// https://learnopengl.com/Guest-Articles/2021/CSM
// @note calling multiple times recalculates
float shadowness(float NdotL, vec3 position){
    // Select shadow correct projection for csm
    float frag_dist = abs((view * vec4(position, 1.0)).z);

    int layer = CASCADE_NUM;
    for (int i = 0; i < CASCADE_NUM; ++i)
    {
        if (frag_dist < shadow_cascade_distances[i])
        {
            layer = i;
            break;
        }
    }

    float shadowness = calculateShadowness(NdotL, position, layer);

    // Blend between shadow maps as suggested by, seems a bit expensive:
    // https://learn.microsoft.com/en-us/windows/win32/dxtecharts/cascaded-shadow-maps
    if(layer > 0) {
        float delta = frag_dist - shadow_cascade_distances[layer - 1];
        if(delta <= CSM_BLEND_BAND) { // Do blending
            float blend = smoothstep(CSM_BLEND_BAND, 0.0, delta);
            shadowness = mix(shadowness, calculateShadowness(NdotL, position, layer - 1), blend);
        }
    }

    return shadowness;
}