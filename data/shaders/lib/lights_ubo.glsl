layout(std140, binding=3) uniform Lights 
{
    vec3 light_positions[MAX_LIGHTS];  // MAX_LIGHTS * 16
    vec3 light_radiances[MAX_LIGHTS];  // MAX_LIGHTS * 16
};