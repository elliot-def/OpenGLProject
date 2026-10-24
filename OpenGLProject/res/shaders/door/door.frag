#version 330 core

// ─── door.frag : porte interactive (matiere unie) ───
// Meme jeu d'uniforms de lumiere que severallights/wall (LightManager::
// applyToShader fonctionne tel quel). L'albedo est une couleur unie (pas de
// texture). Le surlignage d'interaction n'est plus un glow emissif : il est
// rendu par le contour (outline) dessine cote C++ (Door::draw).

out vec4 FragColor;

struct DirLight {
    vec3 direction;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct PointLight {
    vec3 position;

    float constant;
    float linear;
    float quadratic;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct SpotLight {
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;

    float constant;
    float linear;
    float quadratic;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform int numberLightSources;

// Pièces intérieures : chaque lumière est assignée à une pièce par
// uLightRoom[i] (LightManager) et n'éclaire que les fragments de CETTE
// pièce — la lumière ne traverse pas les murs. -1 = lumière libre.
// uRoomCount = nombre de pièces valides (0 = aucune).
uniform int   uRoomCount;
uniform vec3  uRoomMin[MAX_ROOMS];
uniform vec3  uRoomMax[MAX_ROOMS];
uniform int   uLightRoom[MAX_POINT_LIGHTS];

bool roomAllows(int lightIdx, vec3 fragPos) {
    int r = uLightRoom[lightIdx];
    if (r < 0 || r >= uRoomCount) return true;
    return all(greaterThanEqual(fragPos, uRoomMin[r])) && all(lessThanEqual(fragPos, uRoomMax[r]));
}

in vec3 FragPos;
in vec3 Normal;

uniform vec3 viewPos;

#include "fog.glsl"
uniform float uNoFog;   // 1 = pas de brouillard (tete du soleil, visible de loin)
uniform DirLight dirLight;
uniform PointLight lightSources[MAX_POINT_LIGHTS];
uniform SpotLight spotLight;
uniform SpotLight sunSpot;   // soleil (LightManager::aimSunSpot)

// Matiere unie de la porte.
uniform vec3 uColor;

// Bois verni : spéculaire léger mais visible.
const float SHININESS = 32.0;
const float SPECULAR_STRENGTH = 0.15;

// function prototypes
vec3 calcDirLight(DirLight light, vec3 normal, vec3 viewDir);
vec3 calcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir);
vec3 calcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir);

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    // phase 1: directional lighting
    vec3 result = calcDirLight(dirLight, norm, viewDir);

    // phase 2: point lights (lumiere interieure bloquee hors de la piece)
    for (int i = 0; i < numberLightSources; i++)
        result += (!roomAllows(i, FragPos))
                ? vec3(0.0)
                : calcPointLight(lightSources[i], norm, FragPos, viewDir);

    // phase 3: spot light
    result += calcSpotLight(spotLight, norm, FragPos, viewDir);
    result += calcSpotLight(sunSpot, norm, FragPos, viewDir);

    FragColor = vec4(uNoFog > 0.5 ? result : applyFog(result, length(viewPos - FragPos)), 1.0);
}

vec3 calcDirLight(DirLight light, vec3 normal, vec3 viewDir)
{
    vec3 lightDir = normalize(-light.direction);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), SHININESS);
    vec3 ambient = light.ambient * uColor;
    vec3 diffuse = light.diffuse * diff * uColor;
    vec3 specular = light.specular * spec * SPECULAR_STRENGTH;
    return (ambient + diffuse + specular);
}

vec3 calcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), SHININESS);
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    vec3 ambient = light.ambient * uColor;
    vec3 diffuse = light.diffuse * diff * uColor;
    vec3 specular = light.specular * spec * SPECULAR_STRENGTH;
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    return (ambient + diffuse + specular);
}

vec3 calcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), SHININESS);
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
    vec3 ambient = light.ambient * uColor;
    vec3 diffuse = light.diffuse * diff * uColor;
    vec3 specular = light.specular * spec * SPECULAR_STRENGTH;
    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;
    return (ambient + diffuse + specular);
}
