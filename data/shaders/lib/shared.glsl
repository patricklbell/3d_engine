layout(std140, binding = 0) uniform Shared
{
    mat4  projection;
    mat4  view;             // 4x16 - 0
    mat4  vp;               // 4x16 - 64
    vec3  sun_direction;    // 16   - 128
    vec3  sun_color;        // 16   - 144
    vec3  camera_position;  // 16   - 160
    float time;             // 4    - 172
    ivec2 window_size;      // 8    - 176
    float far_plane;        // 4    - 184
    float exposure;         // 4    - 188
};