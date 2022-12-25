#begin VERTEX

layout (location = 0) in vec3 in_vertex;
layout (location = 3) in vec2 in_texcoord;

out vec2 texcoord;

void main(){
    gl_Position = vec4(in_vertex,1.0);
    texcoord = in_texcoord;
}

#end

#begin FRAGMENT
#macro KARIS 1

// https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom
// This shader performs downsampling on an image,
// as taken from Call Of Duty method, presented at ACM Siggraph 2014.
// This particular method was customly designed to eliminate
// "pulsating artifacts and temporal stability issues".

// Remember to add bilinear minification filter for this texture!
// Remember to use a floating-point texture format (for HDR)!
// Remember to use edge clamping for this texture!
in vec2 texcoord;

out vec3 out_color;

uniform vec2 resolution;
layout (location = 0) uniform sampler2D image;
#if KARIS
uniform int is_mip0;

vec3 pow_vec3(vec3 v, float p)
{
    return vec3(pow(v.x, p), pow(v.y, p), pow(v.z, p));
}

const float inv_gamma = 1.0 / 1.6;
vec3 to_srgb(vec3 v) { return pow_vec3(v, inv_gamma); }

float rgb_to_luminance(vec3 col)
{
    return dot(col, vec3(0.2126f, 0.7152f, 0.0722f));
}

float karis_average(vec3 col)
{
    // Formula is 1 / (1 + luma)
    float luma = rgb_to_luminance(to_srgb(col)) * 0.25f;
    return 1.0f / (1.0f + luma);
}
#endif

void main()
{
    vec2 texel_size = 1.0 / resolution;
    float x = texel_size.x;
    float y = texel_size.y;

    // Take 13 samples around current texel:
    // a - b - c
    // - j - k -
    // d - e - f
    // - l - m -
    // g - h - i
    // === ('e' is the current texel) ===
    vec3 a = texture(image, vec2(texcoord.x - 2*x, texcoord.y + 2*y)).rgb;
    vec3 b = texture(image, vec2(texcoord.x,       texcoord.y + 2*y)).rgb;
    vec3 c = texture(image, vec2(texcoord.x + 2*x, texcoord.y + 2*y)).rgb;

    vec3 d = texture(image, vec2(texcoord.x - 2*x, texcoord.y)).rgb;
    vec3 e = texture(image, vec2(texcoord.x,       texcoord.y)).rgb;
    vec3 f = texture(image, vec2(texcoord.x + 2*x, texcoord.y)).rgb;

    vec3 g = texture(image, vec2(texcoord.x - 2*x, texcoord.y - 2*y)).rgb;
    vec3 h = texture(image, vec2(texcoord.x,       texcoord.y - 2*y)).rgb;
    vec3 i = texture(image, vec2(texcoord.x + 2*x, texcoord.y - 2*y)).rgb;

    vec3 j = texture(image, vec2(texcoord.x - x, texcoord.y + y)).rgb;
    vec3 k = texture(image, vec2(texcoord.x + x, texcoord.y + y)).rgb;
    vec3 l = texture(image, vec2(texcoord.x - x, texcoord.y - y)).rgb;
    vec3 m = texture(image, vec2(texcoord.x + x, texcoord.y - y)).rgb;

#if KARIS
    // Check if we need to perform Karis average on each block of 4 samples
    vec3 groups[5];
    switch (is_mip0)
    {
    case 1:
        // We are writing to mip 0, so we need to apply Karis average to each block
        // of 4 samples to prevent fireflies (very bright subpixels, leads to pulsating
        // artifacts).
        groups[0] = (a+b+d+e) * (0.125f/4.0f);
        groups[1] = (b+c+e+f) * (0.125f/4.0f);
        groups[2] = (d+e+g+h) * (0.125f/4.0f);
        groups[3] = (e+f+h+i) * (0.125f/4.0f);
        groups[4] = (j+k+l+m) * (0.5f/4.0f);
        groups[0] *= karis_average(groups[0]);
        groups[1] *= karis_average(groups[1]);
        groups[2] *= karis_average(groups[2]);
        groups[3] *= karis_average(groups[3]);
        groups[4] *= karis_average(groups[4]);
        out_color = groups[0]+groups[1]+groups[2]+groups[3]+groups[4];
        break;
    default:
#endif
        // Apply weighted distribution:
        // 0.5 + 0.125 + 0.125 + 0.125 + 0.125 = 1
        // a,b,d,e * 0.125
        // b,c,e,f * 0.125
        // d,e,g,h * 0.125
        // e,f,h,i * 0.125
        // j,k,l,m * 0.5
        // This shows 5 square areas that are being sampled. But some of them overlap,
        // so to have an energy preserving downsample we need to make some adjustments.
        // The weights are the distributed, so that the sum of j,k,l,m (e.g.)
        // contribute 0.5 to the final color output. The code below is written
        // to effectively yield this sum. We get:
        // 0.125*5 + 0.03125*4 + 0.0625*4 = 1
        out_color  = e*0.125;
        out_color += (a+c+g+i)*0.03125;
        out_color += (b+d+f+h)*0.0625;
        out_color += (j+k+l+m)*0.125;
        out_color = max(out_color, 0.0001f);
#if KARIS
        break;
    }
#endif

}

#end