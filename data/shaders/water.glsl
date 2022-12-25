#begin VERTEX

layout (location = 0) in vec3 in_vertex;
layout (location = 3) in vec2 in_texcoord;

out VS_TCS {
  vec2 texcoord;
} vs_out;

void main() {
    gl_Position = vec4(in_vertex, 1.0);
    vs_out.texcoord = in_texcoord;
}
#end

#begin TESSELLATION_CONTROL
layout(vertices=3) out;

in VS_TCS {
  vec2 texcoord;
} tcs_in[];

out TCS_TES {
  vec2 texcoord;
} tcs_out[];

const float factor = 4.0;

#load lib/shared.glsl
uniform mat4 model;

void main()
{
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    tcs_out[gl_InvocationID].texcoord = tcs_in[gl_InvocationID].texcoord;

    if (gl_InvocationID == 0) {
        const int MIN_TESS_LEVEL = 4;
        const int MAX_TESS_LEVEL = 16;
        const float MIN_DISTANCE = 1;
        const float MAX_DISTANCE = 50;

        // @todo Obviously needs some optimising but this will do for now
        vec4 eye00 = view * model * gl_in[0].gl_Position;
        vec4 eye01 = view * model * gl_in[1].gl_Position;
        vec4 eye10 = view * model * gl_in[2].gl_Position;
        
        float d00 = clamp( (abs(eye00.z) - MIN_DISTANCE) / (MAX_DISTANCE-MIN_DISTANCE), 0.0, 1.0 );
        float d01 = clamp( (abs(eye01.z) - MIN_DISTANCE) / (MAX_DISTANCE-MIN_DISTANCE), 0.0, 1.0 );
        float d10 = clamp( (abs(eye10.z) - MIN_DISTANCE) / (MAX_DISTANCE-MIN_DISTANCE), 0.0, 1.0 );

        // @note tesselation levels are in the middle of the triangle edge so this is techinically incorrect, see
        // https://www.khronos.org/opengl/wiki/Tessellation
        gl_TessLevelOuter[0] = mix( MAX_TESS_LEVEL, MIN_TESS_LEVEL, min(d00, d01));
        gl_TessLevelOuter[1] = mix( MAX_TESS_LEVEL, MIN_TESS_LEVEL, min(d01, d10));
        gl_TessLevelOuter[2] = mix( MAX_TESS_LEVEL, MIN_TESS_LEVEL, min(d10, d00));

        gl_TessLevelInner[0] = max(max(gl_TessLevelOuter[0], gl_TessLevelOuter[1]), gl_TessLevelOuter[2]);
    }
}
#end

#begin TESSELLATION_EVALUATION
layout(triangles, equal_spacing, ccw) in;

in TCS_TES {
  vec2 texcoord;
} tes_in[];

out TES_FS {
  vec2 texcoord;
  vec2 untranslated_texcoord;
  float height;
  vec3 position;
  vec3 normal;
  vec3 binormal;
  vec3 tangent;
  vec4 shadow_coord;
} tes_out;

uniform mat4 model;
uniform mat4 shadow_mvp;

#load lib/shared.glsl
#load lib/waves.glsl

void main()
{ 
    vec2 texcoord  = tes_in[0].texcoord * gl_TessCoord[0];
    texcoord += tes_in[1].texcoord * gl_TessCoord[1];
    texcoord += tes_in[2].texcoord * gl_TessCoord[2];

    vec4 vertex = gl_in[0].gl_Position * gl_TessCoord[0];
    vertex     += gl_in[1].gl_Position * gl_TessCoord[1];
    vertex     += gl_in[2].gl_Position * gl_TessCoord[2];

    vec3 o_vertex = vertex.xyz;
    waves(vertex.xyz, texcoord, time, vertex.xyz, tes_out.normal, tes_out.tangent, tes_out.binormal);

    tes_out.position = (model * vertex).xyz;
    tes_out.normal   = (model * vec4(tes_out.normal,   0.0)).xyz;
    tes_out.binormal = (model * vec4(tes_out.binormal, 0.0)).xyz;
    tes_out.tangent  = (model * vec4(tes_out.tangent,  0.0)).xyz;

    tes_out.height = vertex.y - o_vertex.y;
    tes_out.shadow_coord = shadow_mvp * vertex;
    tes_out.texcoord = texcoord;
    tes_out.untranslated_texcoord = texcoord + (vertex.xz - o_vertex.xz) / 50.0; // @hardcoded

    gl_Position = vp * vec4(tes_out.position, 1.0);
}
#end

#begin FRAGMENT
#macro SHADOWS 0
#macro VOLUMETRICS 1
#macro RAYMARCH_SSR 0

#load lib/constants.glsl
#load lib/shared.glsl
#load lib/utilities.glsl
#load lib/pbr.glsl

#if VOLUMETRICS
#load lib/volumetrics.glsl
#endif

#define EDGE_SCALE      1.252

#define SPEED           5.0
#define BEER_LAW_FACTOR 0.01

in TES_FS {
  vec2 texcoord;
  vec2 untranslated_texcoord;
  float height;
  vec3 position;
  vec3 normal;
  vec3 binormal;
  vec3 tangent;
  vec4 shadow_coord;
} vs_in;

layout (location = 0) out vec4 out_color;

layout (binding = 0) uniform sampler2D hdr_map;
layout (binding = 1) uniform sampler2D depth_map;
// Use pre generated noise from
// https://www.shadertoy.com/view/XdXBRH
layout (binding = 2) uniform sampler2D noise_map;
layout (binding = 3) uniform sampler2D foam_map;
layout (binding = 4) uniform sampler2D normals1_map;
layout (binding = 11) uniform sampler2D normals2_map;
layout (binding = 12) uniform sampler2D collider_map;

uniform vec3 surface_col;
uniform vec3 floor_col;
uniform float floor_height;
uniform float peak_height;
uniform float extinction_coefficient;
uniform vec3 refraction_tint_col;
uniform float specular_intensity;
uniform float reflectance;
uniform float roughness;
uniform float specular;
uniform float refraction_distortion_factor;
uniform float refraction_height_factor;
uniform float refraction_distance_factor;
uniform float foam_height_start;
uniform float foam_tilling;
uniform float foam_angle_exponent;
uniform float foam_brightness;
uniform vec4 normal_scroll_direction;
uniform vec2 normal_scroll_speed;
uniform vec2 tilling_size;
uniform vec4 ssr_settings;

float calcDepthFade(float diff){
    return exp(-diff*BEER_LAW_FACTOR);
}

vec3 ssrReflection(vec3 origin, vec3 direction, vec2 screencoord, float NdotV, vec4 settings) {
#if RAYMARCH_SSR
    float scene_z = 0;
    float step = 0;
    float hit_steps = settings.y;
    vec3 ray_p = origin;
    vec4 ray_uv = vec4(0,0,0,0);

    while(step < settings.y) {	
        ray_p += settings.x*direction.xyz;

        ray_uv = vp * vec4(ray_p, 1.0);
        ray_uv.xy /= (abs(ray_uv.w) < EPSILON) ? EPSILON : ray_uv.w;
        ray_uv.xy = ray_uv.xy * 0.5 + 0.5; 

        scene_z = depthToDist(texture(depth_map, ray_uv.xy).r);

        if (scene_z <= length(ray_p - camera_position)) {
            hit_steps = step;
            step = settings.y;				
        } else {
            step++;
        }
    }

    if (hit_steps < settings.y) {
        step = 0;
        while(step < settings.z) {
            ray_p -= direction.xyz * settings.x / settings.z;

            ray_uv = vp * vec4(ray_p, 1.0);
            ray_uv.xy /= (abs(ray_uv.w) < EPSILON) ? EPSILON : ray_uv.w;
            ray_uv.xy = ray_uv.xy * 0.5 + 0.5; 

            scene_z = depthToDist(texture(depth_map, ray_uv.xy).r);

            if (scene_z <= length(ray_p - camera_position)) {
                step = settings.z;
            } else {
                step++;
            }
        }
    }

    //vec3 ssrReflectionNormal = DecodeSphereMap(NormalMap.Sample(PointClampSampler, ray_uv.xy).xy);
    vec2 distance_factor = vec2(abs(screencoord.x - 0.5), abs(screencoord.y - 0.5)) * 2;
    float factor = (1.0 - abs(NdotV))
                 * (1.0 - hit_steps / settings.y)
                 * clamp(1.0 - distance_factor.x - distance_factor.y, 0.0, 1.0)
                 * (1.0 / (1.0 + abs(scene_z - (ray_p.z - camera_position.z)) * settings.w));
                 //* (1.0 - clamp(dot(ssrReflectionNormal, normal), 0.0, 1.0));

    vec3 reflection = texture(hdr_map, ray_uv.xy).rgb;
#endif

#if IBL
    vec3 skybox = texture(irradiance_map, direction).rgb; 
#else
    vec3 skybox = vec3(0.5);
#endif
#if RAYMARCH_SSR
    return mix(skybox, reflection, factor);
#else
    return skybox;
#endif
}

// Exponential integral approximation https://mathworld.wolfram.com/ExponentialIntegral.html
float Ei(float z) {
    float z2 = z  * z;
    float z3 = z2 * z;
    float z4 = z3 * z;
    float z5 = z4 * z;
    return EULER_MASCHERONI + log(EPSILON + abs(z)) + z + z2/4.f + z3/18.f + z4/96.f + z5/600.f;
}

// Ambient contribution from top and bottom light planes for uniform medium
// https://patapom.com/topics/Revision2013/Revision%202013%20-%20Real-time%20Volumetric%20Rendering%20Course%20Notes.pdf
vec3 computeAmbientColor(float height, vec3 isotropic_light_top, vec3 isotropic_light_bottom, float extinction) {
    float Ht = max(peak_height - height, EPSILON); // Distance from top
    float a = -extinction * Ht;
    vec3 isotropic_scattering_top = isotropic_light_top * max(0.0, exp(a) - a*Ei(a));

    float Hb = max(floor_height + height, EPSILON); // Distance to floor
    a = -2.0*extinction * Hb;
    vec3 isotropic_scattering_bottom = isotropic_light_bottom * max(0.0, exp(a) - a*Ei(a));
    return isotropic_scattering_top + isotropic_scattering_bottom;
}

void main(){
    float t = time * SPEED;
    vec2 screencoord = gl_FragCoord.xy / window_size.xy;

    // Normalize since interpolation makes them slightly off, @note is probably overkill could just output TBN from TES
    vec3 normal = normalize(vs_in.normal);
    vec3 tangent = normalize(vs_in.tangent);
    vec3 binormal = normalize(vs_in.binormal);
    mat3 TBN = mat3(tangent, normal, binormal);

    vec2 n1_uv = tilling_size*vs_in.texcoord + t*normal_scroll_direction.xy*normal_scroll_speed.x;
    vec2 n2_uv = tilling_size*vs_in.texcoord + t*normal_scroll_direction.zw*normal_scroll_speed.y;
    vec3 n1 = texture(normals1_map, n1_uv).rgb*2.0 - 1.0;
    vec3 n2 = texture(normals2_map, n2_uv).rgb*2.0 - 1.0;

    vec3 N  = TBN * n1;
    N      += TBN * n2;
    N       = normalize(N);

    vec3 V = normalize(camera_position - vs_in.position);
    vec3 L = -sun_direction;
    vec3 H = normalize(V + L);
    float NdotV = abs(dot(N, V)) + EPSILON;
    float NdotH = max(dot(N, H), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    vec3  F0 = vec3(0.16 * reflectance * reflectance);
    float NDF = distributionGGX(NdotH, roughness);   
    float G   = geometrySmithGGXCorrelated(NdotV, NdotL, roughness*roughness);
    vec3  F   = fresnelSchlick(HdotV, F0);

    float specular_noise = textureLod(noise_map, n1_uv * 0.5, 0.0).r;
    specular_noise      *= textureLod(noise_map, n2_uv * 0.5, 0.0).r;
    specular_noise      *= textureLod(noise_map, vs_in.texcoord.xy * 0.5, 0.0).r;
    specular_noise       = pow(specular_noise, 3);
    vec3 specular_col = G * NDF * F * sun_color * NdotL * specular_intensity * specular_noise;

    //vec3 R = reflect(-V, N);
    //vec3 surface_col = ssrReflection(vs_in.position, R, screencoord, NdotV, ssr_settings)*surface_col * (1.0 - F) / PI;
#if IBL
    vec3 irradiance = texture(irradiance_map, N).rgb;
#else
    vec3 irradiance = vec3(0.5);
#endif
    vec3 surface_col = irradiance*computeAmbientColor(vs_in.height, surface_col, floor_col, extinction_coefficient);
    
//    float water_depth = gl_FragCoord.z;
//    float water_d = depthToDist(water_depth);
//    float scene_d = depthToDist(texture(depth_map, screencoord).r);
//    float depth_fade = 1.0 - calcDepthFade(water_d, scene_d);
//    depth_fade = max(depth_fade, 1.0 - texture(collider_map, vs_in.texcoord).r);

    float scene_depth = texture(depth_map, screencoord).r;
    vec3 scene_p = depthToWorldPosition(scene_depth, screencoord, inverse(vp));
    float depth_fade = calcDepthFade(distance(scene_p, vs_in.position));

    vec2 distortion_noise_sample = vec2(textureLod(noise_map, vec2(0.001*t) + vs_in.texcoord.xy*2.0, 0.0).r);
    vec2 distortion_noise = N.xz + N.xy + 0.5*distortion_noise_sample;
    vec2 distorted_uv = screencoord + 2.0*distortion_noise*refraction_distortion_factor*(1 - 2*length(screencoord - 0.5))/length(camera_position - vs_in.position);
    float distorted_depth = texture(depth_map, distorted_uv).r; // @todo fix edge artifacts due to difference between depth and screen color
    vec3 distorted_p = depthToWorldPosition(distorted_depth, distorted_uv, inverse(vp));
    vec3 refraction_p = (distorted_p.y < vs_in.position.y) ? distorted_p : scene_p;
    vec3 refraction_col = texture(hdr_map, distorted_uv).rgb*refraction_tint_col;
    refraction_col = mix(refraction_col, refraction_tint_col, clamp((vs_in.position.y - refraction_p.y)/refraction_height_factor, 0.0, 1.0));

    float wavetop_reflection_factor = pow(1.0 - clamp(dot(normal, V), 0.0, 1.0), 3.0);
    vec3 diffuse_col = mix(refraction_col, surface_col, clamp(clamp(length(camera_position - vs_in.position)/refraction_distance_factor, 0.0, 1.0) + wavetop_reflection_factor, 0.0, 1.0));

    vec3 foam_col = texture(foam_map, (n1_uv + n2_uv)*foam_tilling).rgb;
    float foam_noise = texture(noise_map, vs_in.texcoord*0.01).r;
    float foam_amount = clamp(vs_in.height - foam_height_start, 0.0, 1.0) * pow(normal.y, foam_angle_exponent) * foam_noise;
    float collision = smoothstep(0.0, 1.0, texture(collider_map, vs_in.untranslated_texcoord).r);
    foam_amount = max(foam_amount, (1.0 - collision)*(n1.z + n1.x*max(sin(collision - n2.x*t), 0.0)));
    //foam_amount += 0.1*pow(max(depth_fade - 0.5, 0.0), 3);
    foam_amount = clamp(foam_amount, 0.0, 1.0);

    vec3 col = diffuse_col + specular_col + foam_col*foam_brightness*foam_amount;

#if VOLUMETRICS
    col = addInscatteredVolumetrics(col, gl_FragCoord.xyz);
#endif

    out_color = vec4(col, 1.0); // You could set alpha here
}

#end