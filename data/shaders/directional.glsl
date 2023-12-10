#begin VERTEX
layout (location = 0) in vec3 position;

void main()
{
    gl_Position = vec4(position, 1.0);
}
#end

#begin FRAGMENT
out vec4 out_color;

uniform sampler2D   positionMap;
uniform sampler2D   diffuseMap;
uniform sampler2D   normalMap;
uniform vec2        screenSize;
uniform vec3        lightColor;
uniform vec3        lightDirection;
uniform vec3        cameraPosition;

#define AMBIENT_INTENSITY  0.1
#define DIFFUSE_INTENSITY  0.8
#define SPECULAR_EXP       30
#define SPECULAR_INTENSITY 4

vec3 calcDirectionalLight(vec3 position, vec3 normal)
{
    vec3 ambientColor = lightColor * AMBIENT_INTENSITY;
    float diffuseFactor = dot(normal, -lightDirection);

    vec3 diffuseColor = vec3(0, 0, 0);
    vec3 specularColor = vec3(0, 0, 0);

    if (diffuseFactor > 0) {
        diffuseColor = lightColor * DIFFUSE_INTENSITY * diffuseFactor;
        vec3 viewDirection = normalize(cameraPosition - position);
        vec3 halfwayDirection = normalize(-lightDirection + viewDirection);
        float specularFactor = dot(normal, halfwayDirection);
        if (specularFactor > 0) {
            specularFactor = pow(specularFactor, SPECULAR_EXP);
            specularColor = lightColor * SPECULAR_INTENSITY * specularFactor;
        }
    }

    return (ambientColor + diffuseColor + specularColor);
}

vec2 calcTexCoord()
{
   return gl_FragCoord.xy / screenSize;
} 

void main()
{
    vec2 texcoord = calcTexCoord();
    vec3 position = texture(positionMap, texcoord).xyz;
    vec3 color = texture(diffuseMap, texcoord).xyz;
    vec3 normal = normalize(texture(normalMap, texcoord).xyz);

    out_color = vec4(color * calcDirectionalLight(position, normal), 1.0);
}

#end