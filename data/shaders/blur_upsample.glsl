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
// https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom
// This shader performs upsampling on an image,
// as taken from Call Of Duty method, presented at ACM Siggraph 2014.

// Remember to add bilinear minification filter for this texture!
// Remember to use a floating-point texture format (for HDR)!
// Remember to use edge clamping for this texture!
in vec2 texcoord;

out vec3 out_color;

layout (location = 0) uniform sampler2D image;
const float filter_radius = 0.005f;

void main()
{
    // The filter kernel is applied with a radius, specified in texture
    // coordinates, so that the radius will vary across mip resolutions.
    float x = filter_radius;
    float y = filter_radius;

    // Take 9 samples around current texel:
    // a - b - c
    // d - e - f
    // g - h - i
    // === ('e' is the current texel) ===
    vec3 a = texture(image, vec2(texcoord.x - x, texcoord.y + y)).rgb;
    vec3 b = texture(image, vec2(texcoord.x,     texcoord.y + y)).rgb;
    vec3 c = texture(image, vec2(texcoord.x + x, texcoord.y + y)).rgb;

    vec3 d = texture(image, vec2(texcoord.x - x, texcoord.y)).rgb;
    vec3 e = texture(image, vec2(texcoord.x,     texcoord.y)).rgb;
    vec3 f = texture(image, vec2(texcoord.x + x, texcoord.y)).rgb;

    vec3 g = texture(image, vec2(texcoord.x - x, texcoord.y - y)).rgb;
    vec3 h = texture(image, vec2(texcoord.x,     texcoord.y - y)).rgb;
    vec3 i = texture(image, vec2(texcoord.x + x, texcoord.y - y)).rgb;

    // Apply weighted distribution, by using a 3x3 tent filter:
    //  1   | 1 2 1 |
    // -- * | 2 4 2 |
    // 16   | 1 2 1 |
    out_color = e*4.0;
    out_color += (b+d+f+h)*2.0;
    out_color += (a+c+g+i);
    out_color *= 1.0 / 16.0;
}

#end