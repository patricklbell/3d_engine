#load lib/constants.glsl
#load lib/pbr_helpers.glsl

#load lib/shared.glsl
#if SHADOWS
#load lib/shadows.glsl
#endif
#if LIGHTS
#load lib/lights_ubo.glsl
#endif

#macro IBL 1
#if IBL
layout(binding = 7) uniform samplerCube irradiance_map;
layout(binding = 8) uniform samplerCube prefiltered_map;
layout(binding = 9) uniform sampler2D   brdf_lut_map;  
#endif

vec3 brdfDirect(vec3 N, vec3 V, float NdotV, vec3 F0, vec3 L, vec3 radiance, vec3 albedo, float roughness, float metallic) {
    vec3 H = normalize(V + L);
    float NdotH = max(dot(N, H), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    float NDF = distributionGGX(NdotH, roughness);   
    float G   = geometrySmith(NdotV, NdotL, roughness);       
    vec3  F   = fresnelSchlick(HdotV, F0);

    // Actually calculate Cook-Torrance brdf integral for one light direction
    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL + EPSILON; // stops divide by zero
    vec3 specular     = numerator / denominator;

    // kS is equal to Fresnel
    vec3 kS = F;
    // for energy conservation, the diffuse and specular light can't
    // be above 1.0 (unless the surface emits light); to preserve this
    // relationship the diffuse component (kD) should equal 1.0 - kS.
    vec3 kD = vec3(1.0) - kS;
    // multiply kD by the inverse metalness such that only non-metals 
    // have diffuse lighting, or a linear blend if partly metal (pure metals
    // have no diffuse light).
    kD *= 1.0 - metallic;

    return (kD * albedo / PI + specular) * radiance * NdotL;
}

const float MAX_REFLECTION_LOD = 4.0;
vec3 brdfAmbient(vec3 N, vec3 V, float NdotV, vec3 F0, vec3 irradiance, vec3 albedo, float roughness, float metallic, float reflectivity) {
#if IBL
    vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;

    vec3 R = reflect(-V, N);
    vec3 prefiltered_color = textureLod(prefiltered_map, R, roughness * MAX_REFLECTION_LOD).rgb; 

    vec2 brdf = texture(brdf_lut_map, vec2(NdotV, roughness)).rg;
    vec3 specular = reflectivity * prefiltered_color * (F * brdf.x + brdf.y);

    vec3 diffuse = irradiance * albedo;
    return kD * diffuse + specular;
#else // !IBL
    return albedo * irradiance;
#endif // IBL
}

// Modified from https://learnopengl.com/PBR
vec3 brdf(vec3 P, vec3 N, vec3 albedo, float metallic, float roughness, vec3 illumination) {
    vec3 V = normalize(camera_position - P);
    float NdotV = max(dot(N, V), 0.0);

    // Calculate the amount of fresnel contribution for direct lighting
    // if dia-electric (like plastic) use 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // Direct contributions from sun with shadowing
    vec3 direct = brdfDirect(N, V, NdotV, F0, -sun_direction, sun_color, albedo, roughness, metallic);
#if SHADOWS
    float shadowing = 1.0 - shadowness(max(dot(N, sun_direction), 0.0), P);
    direct *= shadowing;
#endif

#if LIGHTS
    for (int i = 0; i < MAX_LIGHTS; i++) {
        if (any(greaterThan(light_radiances[i], vec3(0)))) {
            vec3 L = light_positions[i] - P;
            float L_sqr = max(dot(L, L), EPSILON);

            vec3 radiance = light_radiances[i] / L_sqr;
            direct += brdfDirect(N, V, NdotV, F0, L / sqrt(L_sqr), radiance, albedo, roughness, metallic);
        }
    }
#endif

    // Irradiance here is the total ambient light hitting the point
#if GLOBAL_ILLUMINATION || !IBL
    vec3 irradiance = illumination;
    float reflectivity = 0.0;
#else
    vec3 irradiance = illumination*texture(irradiance_map, N).rgb;

#if SHADOWS
    float reflectivity = 0.1 + 0.9*shadowing;
#else
    float reflectivity = 1.0;
#endif
#endif

    // Ambient contribution, be it from IBL or just the irradiance x albedo
    vec3 ambient = brdfAmbient(N, V, NdotV, F0, irradiance, albedo, roughness, metallic, reflectivity);
    
    return ambient + direct;
}