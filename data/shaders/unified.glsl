#begin VERTEX
#macro ANIMATED_BONES   0
#macro SHADOWS          0
#macro ALPHA_CLIP       0
#macro VEGETATION       0
#macro SPRITESHEETS     0

layout (location = 0) in vec3  in_vertex;
layout (location = 1) in vec3  in_normal;
layout (location = 2) in vec3  in_tangent;
layout (location = 3) in vec2  in_texcoord;
//layout (location = 4) in vec3 in_bitangent; // @todo make bitangents streamed not calculated in vertex
#if ANIMATED_BONES
layout (location = 5) in ivec4 in_bone_ids;
layout (location = 6) in vec4  in_weights;
#endif
#if VEGETATION
// R - edge stiffness, G - Phase offset, B - overall stiffnes, A - Precomputed ambient occlusion
layout (location = 7) in vec4 in_color;
#endif

out vec3 position;
out vec2 texcoord;
out vec3 T;
out vec3 B;
out vec3 N;

#load lib/shared.glsl
uniform mat4 mvp;
uniform mat4 model;

#if ANIMATED_BONES
#load lib/bone_ubo.glsl
#endif

#if VEGETATION
uniform vec2 wind_direction;
uniform float wind_strength;
#load lib/vegetation.glsl
#endif

#if SPRITESHEETS
uniform float spritesheet_speed;
uniform float spritesheet_time_offset;
uniform int spritesheet_tile_number;
uniform float spritesheet_tile_width;
#endif

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

    gl_Position = mvp * vertex;
	position = (model * vertex).xyz;
	texcoord = in_texcoord;
#if SPRITESHEETS
    int tile_offset = int(floor(time*spritesheet_speed + spritesheet_time_offset));
    texcoord.x += (tile_offset % spritesheet_tile_number) * spritesheet_tile_width;
#endif

#if ANIMATED_BONES
	T = normalize(vec3(model * bone_transform * vec4(in_tangent, 0.0)));
	N = normalize(vec3(model * bone_transform * vec4(in_normal, 0.0)));
#else
    T = normalize(vec3(model * vec4(in_tangent, 0.0)));
	N = normalize(vec3(model * vec4(in_normal, 0.0)));
#endif

	// Re-orthogonalize T with respect to N
    // Didn't seem to have any benefits
	//T = normalize(T - dot(T, N) * N);
	B = cross(N, T);

	// Transpose of perpendicular matrix is inverse
	//TBN = mat3(T, B, N);
}

#end
#begin FRAGMENT

#macro VOLUMETRICS          0
#macro PBR                  0
#macro AO                   0
#macro GLOBAL_ILLUMINATION  0
#macro EMISSIVE             0
#macro DEBUG_SHADOWS        0
#macro METALLIC             0
#macro LIGHTS               0

#load lib/shared.glsl
#load lib/shadows.glsl
#load lib/pbr.glsl
#load lib/blinn.glsl
#if VOLUMETRICS
#load lib/volumetrics.glsl
#endif

in vec3 position;
in vec2 texcoord;
in vec3 T;
in vec3 B;
in vec3 N;

layout (location = 0) out vec4 out_color;

layout(binding = 1) uniform sampler2D normal_map;

#if PBR

layout(binding = 0) uniform sampler2D albedo_map;
uniform vec3 albedo_mult;
layout(binding = 3) uniform sampler2D roughness_map;
uniform float roughness_mult;
#if METALLIC
layout(binding = 2) uniform sampler2D metallic_map;
uniform float metal_mult;
#endif

#else // PBR

// see for default values: https://knowledge.autodesk.com/support/maya/learn-explore/caas/CloudHelp/cloudhelp/2016/ENU/Maya/files/GUID-3EDEB1B3-4E48-485A-9714-9998F6E4944D-htm.html
layout(binding = 0) uniform sampler2D diffuse_map;
uniform vec3 diffuse_mult;
layout(binding = 3) uniform sampler2D specular_map; // For now this just multiplies the diffuse so material is more like a plastic than a metal
uniform float specular_mult;
layout(binding = 2) uniform sampler2D shininess_map; // Cosine power
uniform float shininess_mult;

#endif // PBR

#if AO || GLOBAL_ILLUMINATION
layout(binding = 4) uniform sampler2D ambient_map;
uniform float ambient_mult;
#endif

#if EMISSIVE
layout(binding = 11) uniform sampler2D emissive_map;
uniform vec3 emissive_mult;
#endif

void main() {
#if PBR

    vec4 albedo_alpha = texture(albedo_map, texcoord);
#if ALPHA_CLIP
    if(albedo_alpha.a < 0.5)
        discard;
#endif
    vec3 albedo = albedo_mult*albedo_alpha.rgb;

#else // PBR, kinda pointless but naming is clearer

    vec4 diffuse_alpha = texture(diffuse_map, texcoord);
#if ALPHA_CLIP
    if(diffuse_alpha.a < 0.5)
        discard;
#endif

    vec3 diffuse = diffuse_mult*diffuse_alpha.rgb;
#endif // PBR

    mat3 TBN = mat3(normalize(T),normalize(B),normalize(N));

	// obtain TBN normal from normal map in range [0,1]
    vec3 normal = texture(normal_map, texcoord).rgb;
    // transform normal vector to range [-1,1]
    normal = normalize(normal * 2.0 - 1.0); 
    // Transform normal from tangent to world space
    normal = TBN * normal;

// @debug, renders different layers of csm in colors
#if DEBUG_SHADOWS
#if SHADOWS
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
        
    const vec3 colors[] = {vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0), vec3(1.0, 1.0, 0.0), vec3(1.0, 0.0, 1.0)};
    float NdotL = dot(normal, -sun_direction);

    // show blending
    vec3 color = colors[layer];
    if(layer > 0) {
        float delta = frag_dist - shadow_cascade_distances[layer - 1];
        if(delta <= CSM_BLEND_BAND) { // Do blending
            float blend = smoothstep(CSM_BLEND_BAND, 0.0, delta);
            color = mix(color, colors[layer - 1], blend);
        }
    }

    out_color = vec4((1.0-0.8*shadowness(NdotL, position))*color, 1.0);
#else  // SHADOWS
    out_color = vec4(1.0);
#endif // SHADOWS
    return;
#endif // DEBUG_SHADOWS

#if GLOBAL_ILLUMINATION
    const vec3 ambient = ambient_mult*texture(ambient_map, texcoord).rgb;
#elif AO
    float ambient = ambient_mult*texture(ambient_map, texcoord).r;
#else
    float ambient = 1.0;
#endif

#if PBR

    float roughness  = roughness_mult*texture(roughness_map, texcoord).r; 

#if METALLIC
    float metallic   = metal_mult    *texture(metallic_map, texcoord).r; 
#else
    const float metallic = 0.0;
#endif

    vec3 hdr_color = brdf(position, normal, albedo, metallic, roughness, vec3(ambient));

#else // PBR

    float specular = specular_mult  *texture(specular_map, texcoord).r; 
    float shininess = shininess_mult*texture(shininess_map, texcoord).r; 

#if GLOBAL_ILLUMINATION
    vec3 ambient_col = ambient;
#else
    vec3 ambient_col = diffuse * ambient;
#endif

    vec3 hdr_color = blinnPhongLighting(position, normal, ambient_col, diffuse, diffuse*specular, shininess);

#endif // PBR

#if VOLUMETRICS
    hdr_color = addInscatteredVolumetrics(hdr_color, gl_FragCoord.xyz);
#endif

#if EMISSIVE
    vec3 emissive = emissive_mult * texture(emissive_map, texcoord).rgb;
    hdr_color += 100.0*emissive;
#endif

    out_color = vec4(hdr_color, 1.0);
}

#end