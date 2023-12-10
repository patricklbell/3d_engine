#begin VERTEX

layout (location = 0) in vec3 in_vertex;
layout (location = 3) in vec2 in_texcoord;

out vec2 texcoord;

void main(){
    gl_Position = vec4(in_vertex, 1.0);
    texcoord = in_texcoord;
}

#end
#begin FRAGMENT

in vec2 texcoord;
out vec4 out_color;
  
layout(binding = 0) uniform sampler2D image;
uniform vec2 dimensions;

#load lib/constants.glsl

vec4 make_distance_transform(in vec2 fragcoord) {
    vec4 data = texture(image, texcoord);

    float dist = data.z != 0.0 ? length(dimensions*(data.xy - texcoord)) : SQRT_2;

    return vec4(vec3(dist), 1.0);
}

void main() {
    out_color = make_distance_transform(gl_FragCoord.xy);
}

#end