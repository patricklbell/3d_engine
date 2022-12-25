#begin VERTEX

layout (location = 0) in vec3 position;

void main()
{
    gl_Position = vec4(position, 1.0);
}

#end
#begin FRAGMENT
out vec4 out_color;

uniform sampler2D positionMap;
uniform sampler2D diffuseMap;
uniform sampler2D normalMap;
uniform vec2      screenSize;
uniform vec3      cameraPosition;
uniform vec3      lightPosition;
uniform vec3      lightColor;
uniform float     attenuationConstant;
uniform float     attenuationLinear;
uniform float     attenuationExp;

#define AMBIENT_INTENSITY  0.1
#define DIFFUSE_INTENSITY  0.8
#define SPECULAR_EXP       30
#define SPECULAR_INTENSITY 4


vec4 calcPointLight(vec3 position, vec3 normal)
{
    vec3 lightDirection = position - lightPosition;
    float distance = length(lightDirection);
    lightDirection = normalize(lightDirection);

    vec4 ambientColor = vec4(lightColor, 1.0f) * AMBIENT_INTENSITY;
    float diffuseFactor = dot(normal, -lightDirection);

    vec4 diffuseColor = vec4(0, 0, 0, 0);
    vec4 specularColor = vec4(0, 0, 0, 0);

    if (diffuseFactor > 0) {
        diffuseColor = vec4(lightColor * DIFFUSE_INTENSITY * diffuseFactor, 1.0f);
        vec3 vertexToEye = normalize(cameraPosition - position);
        vec3 lightReflect = normalize(reflect(lightDirection, normal));
        float specularFactor = dot(vertexToEye, lightReflect);
        if (specularFactor > 0) {
            specularFactor = pow(specularFactor, SPECULAR_EXP);
            specularColor = vec4(lightColor * SPECULAR_INTENSITY * specularFactor, 1.0f);
        }
    }
    float attenuation = attenuationConstant + attenuationLinear * distance + attenuationExp * distance * distance;
    return (ambientColor + diffuseColor + specularColor) / attenuation;
}

vec2 CalcTexCoord()
{
   return gl_FragCoord.xy / screenSize;
} 

void main()
{
    vec2 texcoord = CalcTexCoord();
    vec3 position = texture(positionMap, texcoord).xyz;
    vec3 color = texture(diffuseMap, texcoord).xyz;
    vec3 normal = normalize(texture(normalMap, texcoord).xyz);

    out_color = vec4(color, 1.0) * calcPointLight(position, normal);
}

#end