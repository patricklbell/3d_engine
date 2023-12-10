#begin VERTEX
layout (location = 0) in vec3 in_position;

out vec3 position;

uniform mat4 vp; // Should be untranslated

void main() {
    position = in_position;
    gl_Position = vp * vec4(in_position, 1.0);
}
#end
#begin FRAGMENT
in vec3 position;

out vec4 out_color;

layout(binding = 0) uniform samplerCube cubemap;
uniform float roughness;
uniform float texelSphericalArea; // Perface resolution of cubemap

#load lib/constants.glsl
#load lib/noise.glsl
#load lib/pbr_helpers.glsl

// Modified from https://learnopengl.com/PBR/IBL/Specular-IBL
// Takes a Reimann sum of a hemisphere of samples to approximate the 
// irradiance at each sample direction for diffuse shading
const uint SAMPLE_COUNT = 1024u;
void main()
{
    vec3 normal = normalize(position);
    vec3 reflect = normal; // Epic Games approximation, since we can't know look direction
    vec3 view = reflect;

    vec3 up        = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent   = normalize(cross(up, normal));
    vec3 bitangent = cross(normal, tangent);

    float weight = 0.0;   
    vec3 convoluted_color = vec3(0.0);     
    for(uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        vec2 Xi       = hammersley(i, SAMPLE_COUNT);
        vec3 half_dir = importanceSampleGGX(Xi, normal, tangent, bitangent, roughness);
        vec3 light    = normalize(2.0 * dot(view, half_dir) * half_dir - view);

        float NdotL = max(dot(normal, light), 0.0);
        if(NdotL > 0.0)
        {
            float HdotV = max(dot(half_dir, view), 0.0);
            float NdotH = max(dot(normal, half_dir), 0.0);

            float D   = distributionGGX(NdotH, roughness);
            float pdf = (D * NdotH / (4.0 * HdotV)) + 0.0001; 
            float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);
            float mipLevel = roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / texelSphericalArea); 

            convoluted_color += texture(cubemap, light, mipLevel).rgb * NdotL;
            weight           += NdotL;
        }
    }
    convoluted_color = convoluted_color / weight;

    out_color = vec4(convoluted_color, 1.0);
} 

#end