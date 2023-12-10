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
uniform vec2 resolution;
uniform float step;
uniform float num_steps;

vec4 step_jfa(in vec2 fragcoord, in float level) {
    level = clamp(level, 0.0, num_steps - 1.0);
    float step_width = floor(exp2(num_steps - level)+0.5);
    
    float nearest_distance = 9999.0;
    vec2 nearest_coord = vec2(0.0);
    float is_filled = 0.0;

    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 sampler_coord = texcoord + (vec2(x,y) * step_width) / resolution.xy;
                
            vec4 data = texture(image, sampler_coord);
            vec2 seed_coord = data.xy;

            float dist = length(seed_coord - texcoord);
            if (data.z != 0.0 && dist < nearest_distance)
            {
                nearest_distance = dist;
                nearest_coord = seed_coord;
                is_filled = 1.0;
            }
        }
    }
    return vec4(nearest_coord.xy, is_filled, is_filled);
}

void main() {
    out_color = step_jfa(gl_FragCoord.xy, step);
}

#end