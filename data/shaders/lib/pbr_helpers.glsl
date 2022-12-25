#load lib/constants.glsl

// Takes N, halfway vector between view angle and light angle
// Parameter a -> alpha describing roughness usually alpha = roughness^2
float distributionGGX(float NdotH, float roughness)
{
    float nom    = roughness*roughness;
    float NdotH2 = NdotH*NdotH;
	
    float denom  = (NdotH2 * (nom - 1.0) + 1.0);
    denom        = PI * denom * denom;
	
    return nom / denom;
}
// Takes multiplier between 0 -> 1 with 0 being complete microfacet shadowing 
// and roughness
float geometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float geometrySchlickGGXIBL(float NdotV, float roughness)
{
    float a = roughness;
    float k = (a*a) / 2.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

// Takes N, camera view direction, light direction vector, surface roughness
float geometrySmith(float NdotV, float NdotL, float roughness)
{
    float ggx2  = geometrySchlickGGX(NdotV, roughness);
    float ggx1  = geometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

float geometrySmithIBL(float NdotV, float NdotL, float roughness)
{
    float ggx2  = geometrySchlickGGXIBL(NdotV, roughness);
    float ggx1  = geometrySchlickGGXIBL(NdotL, roughness);
	
    return ggx1 * ggx2;
}

// https://google.github.io/filament/Filament.html
float geometrySmithGGXCorrelated(float NdotV, float NdotL, float a) {
    float a2 = a * a;
    float GGXL = NdotV * sqrt((-NdotL * a2 + NdotL) * NdotL + a2);
    float GGXV = NdotL * sqrt((-NdotV * a2 + NdotV) * NdotV + a2);
    float denom = GGXV + GGXL;
    return abs(denom) < EPSILON ? 0.0 : 0.5 / denom;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
} 

vec3 importanceSampleGGX(vec2 Xi, vec3 N, vec3 tangent, vec3 bitangent, float roughness)
{
    float a = roughness*roughness;
	
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a*a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
	
    // from spherical coordinates to cartesian coordinates
    vec3 s;
    s.x = cos(phi) * sinTheta;
    s.y = sin(phi) * sinTheta;
    s.z = cosTheta;
	
    // from tangent-space vector to world-space sample vector
    s = tangent * s.x + bitangent * s.y + N * s.z;
    return normalize(s);
}