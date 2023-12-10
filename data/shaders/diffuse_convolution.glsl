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

#load lib/constants.glsl

// Modified from https://learnopengl.com/PBR/IBL/Diffuse-irradiance
// Takes a Reimann sum of a hemisphere of samples to approximate the 
// irradiance at each sample direction for diffuse shading
void main()
{
    vec3 normal = normalize(position);
    vec3 irradiance = vec3(0.0);  

    vec3 up    = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, normal));
    up         = cross(normal, right); // Normalisation is unnecessary

    const float sample_delta = 0.025;
    float sample_i = 0.0; 
    for(float phi = 0.0; phi < 2.0 * PI; phi += sample_delta)
    {
        for(float theta = 0.0; theta < 0.5 * PI; theta += sample_delta)
        {
            const float st = sin(theta);
            const float ct = sqrt(1 - st*st);
            const float sp = sin(phi);
            const float cp = sqrt(1 - sp*sp);

            // spherical to cartesian (in tangent space)
            vec3 tangent_sample = vec3(st * cp,  st * sp, ct);
            // tangent space to world
            vec3 sample_vec = tangent_sample.x * right + tangent_sample.y * up + tangent_sample.z * normal; 

            irradiance += texture(cubemap, sample_vec).rgb * st * ct; // Scales by area
            sample_i++;
        }
    }
    irradiance = PI * irradiance * (1.0 / float(sample_i));

    out_color = vec4(irradiance, 1.0);
} 

#end