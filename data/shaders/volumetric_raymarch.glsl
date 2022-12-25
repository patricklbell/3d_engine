#begin COMPUTE
#load lib/constants.glsl

layout(local_size_x = LOCAL_SIZE_X, local_size_y = LOCAL_SIZE_Y, local_size_z = LOCAL_SIZE_Z) in;

layout(binding = 0, rgba16f) uniform writeonly image3D raymarched_vol;

layout(binding = 0) uniform sampler3D integrated_vol;

uniform ivec3 vol_size;

#load lib/utilities.glsl

float slice_thickness(float z, float z_max) {
    return FAR*abs(linearToExponentialDistribution((z + 1.0) / z_max) - linearToExponentialDistribution(z / z_max));
}

// https://github.com/Unity-Technologies/VolumetricLighting/blob/master/Assets/VolumetricFog/Shaders/Scatter.compute
// https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite
vec4 accumulate(int z, int z_max, inout vec3 accum_scattering, inout float accum_transmittance, vec3 slice_scattering, float slice_density) {
    const float thickness = slice_thickness(z + 0.5, float(z_max));
    const float slice_transmittance = exp(-slice_density * thickness / (FAR - NEAR));

    vec3 slice_scattering_integral = slice_scattering * (1.0 - slice_transmittance) / max(slice_density, EPSILON);

    accum_scattering += slice_scattering_integral * accum_transmittance;
    accum_transmittance *= slice_transmittance;

    return vec4(accum_scattering, accum_transmittance);
}

void main() {
    vec4 accum_scattering_transmittance = vec4(0.0f, 0.0f, 0.0f, 1.0f);

    ivec3 coord = ivec3(gl_GlobalInvocationID.xy, 0);
    for (int z = 0; z < vol_size.z; z++) {
        coord.z = z;

        vec4 slice_scattering_density = texelFetch(integrated_vol, coord, 0);

        accum_scattering_transmittance = accumulate(z, vol_size.z,
                                                    accum_scattering_transmittance.rgb, 
                                                    accum_scattering_transmittance.a,
                                                    slice_scattering_density.rgb,
                                                    slice_scattering_density.a);

        imageStore(raymarched_vol, coord, accum_scattering_transmittance);
    }
}

#end