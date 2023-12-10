#define LIGHT_POWER       0.3
#define AMBIENT_POWER   0.1

#load lib/constants.glsl

#load lib/shared.glsl
#if SHADOWS
#load lib/shadows.glsl
#endif
#if LIGHTS
#load lib/lights_ubo.glsl
#endif

vec3 blinnPhong(vec3 V, vec3 N, vec3 L, vec3 L_col, vec3 diffuse_col, vec3 specular_col, float shininess) {
    float lambertian = max(dot(L, N), 0.0);
    float specular = 0.0;

    if (lambertian > 0.0) {
        vec3 H = normalize(L + V);
        float HdotN = max(dot(H, N), EPSILON);
        specular = pow(HdotN, shininess);
    }

    return L_col*LIGHT_POWER*(diffuse_col*lambertian + specular_col*specular);
}

// https://en.wikipedia.org/wiki/Blinn%E2%80%93Phong_reflection_model
vec3 blinnPhongLighting(vec3 position, vec3 normal, vec3 ambient_col, vec3 diffuse_col, vec3 specular_col, float shininess)
{
    vec3 V = normalize(camera_position - position);

    vec3 direct = blinnPhong(V, normal, -sun_direction, sun_color, diffuse_col, specular_col, shininess);
#if SHADOWS
    direct *= 1.0 - shadowness(max(dot(normal, -sun_direction), 0.0), position);
#endif

#if LIGHTS
    for (int i = 0; i < MAX_LIGHTS; i++) {
        if (any(greaterThan(light_radiances[i], vec3(0)))) {
            vec3 radiance = light_radiances[i] / dot(light_positions[i] - position, light_positions[i] - position);
            direct += blinnPhong(V, normal, normalize(light_positions[i] - position), radiance, diffuse_col, specular_col, shininess);
        }
    }
#endif

    return AMBIENT_POWER*ambient_col + direct;
}