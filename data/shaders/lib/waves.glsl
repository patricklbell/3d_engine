#define WAVE_SPEED 0.1f

const float damping_factor = 5.0;
const int num_waves = 3;

struct Wave {
    vec3 direction;
    float speed;
    float steepness;
    float wavelength;
    float amplitude;
};

// @todo optimize
void calculateWave(Wave wave, vec3 wave_position, float edge_dampen, float time, out vec3 position, out vec3 normal, out vec3 tangent, out vec3 binormal)
{
    float frequency = 2.0 / wave.wavelength;
    float phaseConstant = wave.speed * frequency;
    float qi = wave.steepness / (wave.amplitude * frequency * num_waves);
    float rad = frequency * dot(wave.direction.xz, wave_position.xz) + time * phaseConstant;
    float sinR = sin(rad);
    float cosR = cos(rad);

    position.x = wave_position.x + qi * wave.amplitude * wave.direction.x * cosR * edge_dampen;
    position.z = wave_position.z + qi * wave.amplitude * wave.direction.z * cosR * edge_dampen;
    position.y = wave.amplitude * (1.0 + sinR) * edge_dampen;

    float waFactor = frequency * wave.amplitude;
    float radN = frequency * dot(wave.direction, position) + time * phaseConstant;
    float sinN = sin(radN);
    float cosN = cos(radN);

    binormal.x = 1 - (qi * wave.direction.x * wave.direction.x * waFactor * sinN);
    binormal.z = -1 * (qi * wave.direction.x * wave.direction.z * waFactor * sinN);
    binormal.y = wave.direction.x * waFactor * cosN;

    tangent.x = -1 * (qi * wave.direction.x * wave.direction.z * waFactor * sinN);
    tangent.z = 1 - (qi * wave.direction.z * wave.direction.z * waFactor * sinN);
    tangent.y = wave.direction.z * waFactor * cosN;

    normal.x = -1 * (wave.direction.x * waFactor * cosN);
    normal.z = -1 * (wave.direction.z * waFactor * cosN);
    normal.y = 1 - (qi * waFactor * sinN);

    binormal = normalize(binormal);
    tangent = normalize(tangent);
    normal = normalize(normal);
}

void waves(in vec3 in_position, in vec2 in_texcoord, float time, out vec3 position, out vec3 normal, out vec3 tangent, out vec3 binormal)
{
    Wave waves[num_waves];
    waves[0].direction = vec3(0.3, 0, -0.7);
    waves[0].steepness = 1.99;
    waves[0].wavelength = 3.75;
    waves[0].amplitude = 0.85;
    waves[0].speed = 1.21;

    waves[1].direction = vec3(0.5, 0, -0.2);
    waves[1].steepness = 1.79;
    waves[1].wavelength = 4.1;
    waves[1].amplitude = 0.52;
    waves[1].speed = 1.03;

    waves[2].direction = vec3(0.1, 0.0, 0.2);
    waves[2].steepness = 0.8;
    waves[2].wavelength = 2.3;
    waves[2].amplitude = 0.42;
    waves[2].speed = 2.03;

    float damping = 1.0 - pow(clamp(abs(in_texcoord.x - 0.5) / 0.5, 0.0, 1.0), damping_factor);
    damping *= 1.0 - pow(clamp(abs(in_texcoord.y - 0.5) / 0.5, 0.0, 1.0), damping_factor);

    position = vec3(0.0);
    normal = vec3(0.0);
    tangent = vec3(0.0);
    binormal = vec3(0.0);

    for(uint i = 0; i < num_waves; i++)
    {
        vec3 w_position, w_normal, w_tangent, w_binormal;
       
        calculateWave(waves[i], in_position, damping, time, w_position, w_normal, w_tangent, w_binormal);
        position += w_position;
        normal   += w_normal;
        tangent  += w_tangent;
        binormal += w_binormal;
    }

    position -= in_position * (num_waves - 1);
    normal = normalize(normal);
    tangent = normalize(tangent);
    binormal = normalize(binormal);
}