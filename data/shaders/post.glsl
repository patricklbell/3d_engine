#begin VERTEX
layout (location = 0) in vec3 in_position;
layout (location = 3) in vec2 in_texcoord;

out vec3 position;
out vec2 view_ray;
out vec2 texcoord;

#load lib/constants.glsl
uniform vec2 resolution;

void main() {
    position = in_position;
    texcoord = in_texcoord;

    const float aspect_ratio = resolution.x / resolution.y;
    view_ray = vec2(in_position.x*aspect_ratio*tan(FOV*0.5), in_position.y*tan(FOV*0.5));

    gl_Position = vec4(in_position, 1.0);
}

#end
#begin FRAGMENT

#macro BLOOM           1
#macro FXAA            1
#macro SSAO            1
#macro DOF             0
#macro VIGNETTE        1

in vec3 position;
in vec2 texcoord;
in vec2 view_ray;

out vec4 out_color;

#load lib/constants.glsl
#load lib/shared.glsl

uniform vec2 resolution;

layout(binding = 0) uniform sampler2D screen_map;
layout(binding = 1) uniform sampler2D depth_map;
#ifdef BLOOM
const float BLOOM_STRENGTH = 0.02f;
layout(binding = 2) uniform sampler2D bloom_map;
#endif

#if VIGNETTE
#define VIGNETTE_START 0.4
#define VIGNETTE_STRENGTH 0.5
#endif

// Very ghetto ssao that needs work
#if SSAO
layout(binding = 6) uniform sampler3D jitter_vol;
const int AO_MAX_KERNEL_SIZE = 40;
const float AO_SAMPLE_RADIUS = 0.5;
const float AO_STRENGTH = 0.15;
// Code to generate ao_kernel
//std::random_device rd; // obtain a random number from hardware
//std::mt19937 gen(rd()); // seed the generator
//std::uniform_real_distribution<> distr(-1, 1); // define the range
//std::uniform_real_distribution<> distr2(0, 1); // define the range
//for (int i = 0; i < 64; ++i) {
//    glm::vec3 random_vector = glm::normalize(glm::vec3(distr(gen), distr(gen), distr2(gen)));
//    random_vector *= distr2(gen);
//    float scale = float(i) / float(64);
//    float t = glm::mix(0.1f, 1.0f, scale * scale);
//    random_vector = random_vector * t;
//    std::cout << "vec3(" << random_vector << "), ";
//}
const vec3 ao_kernel[64] = {
    vec3(0.0373406, 0.0122081, 0.00243057), vec3(-0.00503597, -0.00715785, 0.00424425), vec3(0.0203578, 0.0587954, 0.0297278), vec3(-0.00681434, 0.0268028, 0.0433335), vec3(-0.0439737, 0.0176514, 0.0122535), vec3(-0.00341573, -0.0554793, 0.0477233), vec3(-7.18759e-05, -0.0175515, 0.00304739), vec3(0.0432174, -0.00734478, 0.0777663), vec3(0.0385759, -0.00166619, 0.0313466), vec3(0.0656093, -0.0414814, 0.0391349), vec3(-0.0747055, -0.0353039, 0.0639407), vec3(-0.00844945, 0.0075601, 0.0109479), vec3(0.0396255, -0.0693224, 0.0873658), vec3(0.0296603, -0.0281473, 0.00998741), vec3(-0.0363591, -0.054137, 0.0137274), vec3(-0.00134769, -0.0998662, 0.0694838), vec3(0.00179341, 0.000218298, 0.00155268), vec3(-0.0750242, 0.0979118, 0.00354298), vec3(0.0588792, 0.0832054, 0.0946417), vec3(-0.0247771, -0.0123608, 0.140685), vec3(-0.0504664, -0.102383, 0.0137935), vec3(0.0479323, -0.010614, 0.000414395), vec3(0.0429127, 0.0676389, 0.0584885), vec3(-0.0205706, -0.130932, 0.0873127), vec3(-0.0763954, 0.0693974, 0.0025651), vec3(0.0392227, -0.160876, 0.0241396), vec3(-0.0193343, -0.151142, 0.152328), vec3(0.109917, 0.106882, 0.125177), vec3(0.0230932, -0.0305792, 0.0326354), vec3(-0.00152914, -0.0018517, 0.00291618), vec3(0.0752254, -0.107237, 0.0301785), vec3(0.126854, 0.127189, 0.0449808), vec3(-0.151894, 0.205365, 0.120098), vec3(0.00178655, 0.00327084, 0.0044552), vec3(0.0301479, 0.0192293, 0.00704896), vec3(0.00914694, 0.20196, 0.194868), vec3(-0.0556878, 0.133273, 0.0303549), vec3(0.214447, -0.140212, 0.195373), vec3(-0.123617, -0.15502, 0.0313221), vec3(-0.149386, -0.0277089, 0.0364906), vec3(0.0971169, -0.110375, 0.103481), vec3(0.355368, -0.0468491, 0.00900307), vec3(-0.300856, 0.0594146, 0.148527), vec3(-0.0764616, -0.393959, 0.290049), vec3(0.173241, -0.0999699, 0.143868), vec3(-0.240792, 0.0675253, 0.156477), vec3(0.146109, -0.158179, 0.146962), vec3(0.0122482, -0.0294594, 0.0113633), vec3(-0.229832, 0.259291, 0.390351), vec3(0.0325702, -0.00783508, 0.0398777), vec3(-0.0521683, -0.0571021, 0.00649676), vec3(-0.3498, 0.452148, 0.261255), vec3(0.169717, 0.290796, 0.135511), vec3(0.642768, 0.0646419, 0.148149), vec3(0.120856, 0.11658, 0.0982047), vec3(0.264666, 0.545325, 0.0320975), vec3(-0.435102, 0.421348, 0.0158116), vec3(0.42311, -0.353223, 0.3732), vec3(-0.00332944, -0.00746772, 0.000218203), vec3(-0.174439, 0.459001, 0.303564), vec3(0.223277, 0.232611, 0.706141), vec3(-0.0899752, 0.130744, 0.163061), vec3(0.668455, -0.0589811, 0.112877), vec3(0.0187402, 0.0476154, 0.0703876)
};
#endif

layout(binding = 3) uniform samplerCube skybox;

#if FXAA
// from: https://github.com/mattdesl/glsl-fxaa/blob/master/fxaa.glsl
#define FXAA_REDUCE_MIN   (1.0 / 128.0)
#define FXAA_REDUCE_MUL   (1.0 / 8.0)
#define FXAA_SPAN_MAX     8.0
//optimized version for mobile, where dependent 
//texture reads can be a bottleneck
vec4 fxaa(sampler2D tex, vec2 coord) {
    vec4 color;
    vec2 inverse_vp = vec2(1.0 / resolution.x, 1.0 / resolution.y);
    vec3 rgbNW = textureLod(tex, (coord + vec2(-1.0, 1.0))*inverse_vp, 0.0).xyz;
    vec3 rgbNE = textureLod(tex, (coord + vec2( 1.0, 1.0))*inverse_vp, 0.0).xyz;
    vec3 rgbSW = textureLod(tex, (coord + vec2(-1.0,-1.0))*inverse_vp, 0.0).xyz;
    vec3 rgbSE = textureLod(tex, (coord + vec2( 1.0,-1.0))*inverse_vp, 0.0).xyz;
    vec4 tex_col = textureLod(tex, coord*inverse_vp, 0.0);
    vec3 rgbM  = tex_col.xyz;

    const vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot(rgbM,  luma);
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
    
    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) *
                          (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);
    
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = min(vec2(FXAA_SPAN_MAX, FXAA_SPAN_MAX),
              max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
              dir * rcpDirMin)) * inverse_vp;
    
    vec3 rgbA = 0.5 * (
        textureLod(tex, coord * inverse_vp + dir * (1.0 / 3.0 - 0.5), 0.0).xyz +
        textureLod(tex, coord * inverse_vp + dir * (2.0 / 3.0 - 0.5), 0.0).xyz);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        textureLod(tex, coord * inverse_vp + dir * -0.5, 0.0).xyz +
        textureLod(tex, coord * inverse_vp + dir * 0.5, 0.0).xyz);

    float lumaB = dot(rgbB, luma);
    if ((lumaB < lumaMin) || (lumaB > lumaMax)) {
        return vec4(rgbA, tex_col.a);
    } else {
        return vec4(rgbB, tex_col.a);
    }
}
#endif

float depthToDist(float d){
  float z = 2.0*d - 1.0;
  return 2.0*NEAR*FAR/(FAR + NEAR - z*(FAR - NEAR));
}

const float A = 0.15;
const float B = 0.50;
const float C = 0.10;
const float D = 0.20;
const float E = 0.02;
const float F = 0.30;
const float W = 11.2;

vec3 uncharted2_tonemap(vec3 x)
{
     return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

vec3 reinhard_tonemap(vec3 x)
{
     return x / (x + vec3(1.0));
}

void main()
{
#if FXAA
    vec3 hdr_color = fxaa(screen_map, gl_FragCoord.xy).xyz;
#else
    vec3 hdr_color = texture(screen_map, texcoord).xyz;
#endif

#if BLOOM
    // Don't fxaa bloom since it is blurred
    vec3 bloom_color = texture(bloom_map, texcoord).rgb;
    hdr_color = mix(hdr_color, bloom_color, BLOOM_STRENGTH);
#endif

#if SSAO || (DOF && BLOOM)
    float dist = depthToDist(texture(depth_map, texcoord).r);
    vec3 view_position = dist*vec3(view_ray, 1.0);
#endif

#if SSAO
    float AO = 0.0;
    for (int i = 0; i < AO_MAX_KERNEL_SIZE ; i++) {
        vec3 noise = texture(jitter_vol, vec3(i) + ao_kernel[i]).xyz;
        vec3 sample_pos = view_position.xyz + noise*ao_kernel[i];
        vec4 offset = vec4(sample_pos, 1.0);
        offset = projection * offset;
        offset.xy /= offset.w;
        offset.xy = 0.5 - offset.xy * 0.5;

        float sample_dist = depthToDist(texture(depth_map, offset.xy).r);

        if (abs(dist - sample_dist) < AO_SAMPLE_RADIUS) {
            AO += step(sample_dist, sample_pos.z);
        }
    }

    AO = AO/float(AO_MAX_KERNEL_SIZE);
    AO = AO*AO;

    hdr_color *= 1.0 - AO_STRENGTH*AO;
#endif

#if DOF && BLOOM
    vec3 focus_point = vec3(0.0, 0.0, depthToDist(texture(depth_map, vec2(0.5, 0.5)).r)); // Make center of scene the focus
    float min_distance = 1.0, max_distance = 4.0 * focus_point.z;
    float blur = smoothstep(min_distance, max_distance, length(view_position - focus_point));
    hdr_color = mix(hdr_color, bloom_color, 0.5*blur);
#endif

    hdr_color /= exposure;

    // Reinhard tone mapping
    //vec3 mapped = reinhard_tonemap(hdr_color);
    vec3 mapped = uncharted2_tonemap(hdr_color);

    // gamma correction 
    const float gamma = 2.2;
    mapped = pow(mapped, vec3(1.0 / gamma));

#if VIGNETTE
    mapped *= 1.0 - VIGNETTE_STRENGTH*smoothstep(VIGNETTE_START, 1.0, distance(texcoord.xy, vec2(0.5, 0.5)));
#endif

    out_color = vec4(mapped, 1.0);
} 

#end