// Pretty much copied from the write up by crytek, it would be good to find a more modern version
// https://developer.nvidia.com/gpugems/gpugems3/part-iii-rendering/chapter-16-vegetation-procedural-animation-and-shading-crysis
vec4 SmoothCurve( vec4 x ) {
    return x * x *( 3.0 - 2.0 * x );
} 

vec4 TriangleWave( vec4 x ) {
    return abs( fract( x + 0.5 ) * 2.0 - 1.0 );
} 

vec4 SmoothTriangleWave( vec4 x ) {
    return SmoothCurve( TriangleWave( x ) ); 
} 

vec3 applyWindToVegetation(vec3 v, vec3 n, float branch_atten, float branch_phase, float edge_atten, vec2 direction, float strength, float time) {
    float fLength = length(v);

    const float fDetailPhase = 0.9;
    // Phases (object, v, branch)    
    float object_phase = dot(v.xyz, vec3(1.0)); 
    branch_phase += object_phase; 
    float v_phase = dot(v.xyz, vec3(fDetailPhase + branch_phase)); 
    // x is used for edges; y is used for branches    
    vec2 wave_t = vec2(time) + vec2(v_phase, branch_phase); 

    const float fSpeed = strength, fDetailAmp = 0.02, fBranchAmp = 0.45;
    // 1.975, 0.793, 0.375, 0.193 are good frequencies    
    vec4 wave = (fract( wave_t.xxyy * vec4(1.975, 0.793, 0.375, 0.193) ) * 2.0 - 1.0 ) * fSpeed; 
    wave = SmoothTriangleWave( wave ); 
    vec2 wave_sum = wave.xz + wave.yw; 
    // Edge (xy) and branch bending (z)
    v.xyz += wave_sum.xxy * vec3((1.0-edge_atten) * fDetailAmp * n.xy, (1.0-branch_atten) * fBranchAmp); 

    //const float fBendScale = 0.1 + 0.1*sin(0.5*time);
    const float fBendScale = 0.001;
    // Bend factor - Wind variation is done on the CPU.    
    float fBF = v.z * fBendScale; 
    // Smooth bending factor and increase its nearby height limit. 
    fBF += 1.0; 
    fBF *= fBF;
    fBF = fBF * fBF - fBF; 
    // Displace position    
    v.xy += direction.xy * fBF; 
    // Rescale 
    v.xyz = normalize(v.xyz)* fLength; 

    return v;
}