#load lib/constants.glsl

float depthToDist(float d){
  float z_ndc = 2.0*d - 1.0; // Convert from 0 -> 1 to NDC
  return (2.0*NEAR*FAR) /(FAR + NEAR - z_ndc*(FAR - NEAR));
}

float distToDepth(float dist) {
	float z_ndc = ((2.0*NEAR*FAR) / dist - (FAR + NEAR)) / (NEAR - FAR);
	return (z_ndc + 1.0) / 2.0; // Convert from NDC to 0 -> 1
}

float linearToExponentialDistribution(float z) {
    return pow(FAR / NEAR, z - 1);
}

float exponentialToLinearDistribution(float z) {
    return max(1 + log2(z) / log2(FAR / NEAR), 0.0);
}

float linearToDepth(float z_linear) {
    return (1.0/z_linear - FAR/NEAR) / (1.0 - FAR/NEAR);
}

float depthToLinear(float z_exp) {
    return 1.0 / (z_exp*(1 - FAR/NEAR) + FAR/NEAR);
}

vec3 depthToWorldPosition(float depth, vec2 uv, mat4 inv_vp) {
    vec4 p = inv_vp * vec4(vec3(uv, depth)*2.0 - 1.0, 1.0);
    p.xyz /= abs(p.w) < EPSILON ? EPSILON :  p.w;

    return p.xyz;
}