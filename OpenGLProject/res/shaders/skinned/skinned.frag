#version 330 core

// ─── skinned.frag : fragment shader identique à model.frag ───

struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct PointLight {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
};

// Champs alignes sur Spotlight::applyToShader (cutOff, pas innerCutOff) :
// sinon les uniforms spotLight.cutOff restent -1 et la flashlight n'echaire
// jamais le modele (no-op silencieux). Identique a model.frag.
struct SpotLight {
    vec3 position;
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
    float cutOff;
    float outerCutOff;
};

#define MAX_LIGHTS 10

in vec3 FragPos;
in vec3 Normal;
in vec3 Color;
in vec2 TexCoords;

out vec4 FragColor;

uniform sampler2D texture_diffuse;
uniform sampler2D texture_specular;

uniform vec3 viewPos;
uniform DirLight dirLight;
uniform SpotLight spotLight;
uniform int numberLightSources;
uniform PointLight lightSources[MAX_LIGHTS];

// Alpha fade 1P : fragments trop proches de la camera deviennent transparents.
// smoothstep(start, end, distance) — voir constants/camera.h.
uniform float uNearFadeStart = 0.08;
uniform float uNearFadeEnd   = 0.35;

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 diffTex, vec3 specTex);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 diffTex, vec3 specTex);
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 diffTex, vec3 specTex);

void main()
{
    vec3 diffTex = vec3(texture(texture_diffuse, TexCoords));
    vec3 specTex = vec3(texture(texture_specular, TexCoords));

    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    vec3 result = vec3(0.0);
    result += CalcDirLight(dirLight, norm, viewDir, diffTex, specTex);
    for (int i = 0; i < numberLightSources && i < MAX_LIGHTS; i++) {
        result += CalcPointLight(lightSources[i], norm, FragPos, viewDir, diffTex, specTex);
    }
    result += CalcSpotLight(spotLight, norm, FragPos, viewDir, diffTex, specTex);

    // Alpha fade progressif base sur la distance a la camera.
    // Les fragments proches (epaules qui traversent la camera en strafe)
    // deviennent transparents au lieu d'etre brutalement tronques.
    float distToCamera = length(viewPos - FragPos);
    float alpha = smoothstep(uNearFadeStart, uNearFadeEnd, distToCamera);
    FragColor = vec4(result, alpha);
}

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 diffTex, vec3 specTex)
{
    vec3 lightDir = normalize(-light.direction);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    return (light.ambient * diffTex)
         + (light.diffuse * diff * diffTex)
         + (light.specular * spec * specTex);
}

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 diffTex, vec3 specTex)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    return (light.ambient * diffTex * attenuation)
         + (light.diffuse * diff * diffTex * attenuation)
         + (light.specular * spec * specTex * attenuation);
}

vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 diffTex, vec3 specTex)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    return (light.ambient * diffTex * attenuation * intensity)
         + (light.diffuse * diff * diffTex * attenuation * intensity)
         + (light.specular * spec * specTex * attenuation * intensity);
}
