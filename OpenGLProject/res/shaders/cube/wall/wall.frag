#version 330 core
out vec4 FragColor;

struct Material {
    sampler2D diffuse;
    sampler2D normal;
    float shininess;
};

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
in vec2 TexCoords;

uniform vec3 viewPos;

#include "fog.glsl"
uniform DirLight dirLight;
uniform PointLight lightSources[MAX_POINT_LIGHTS];
uniform SpotLight spotLight;
uniform SpotLight sunSpot;   // soleil (LightManager::aimSunSpot)
uniform Material material;

#include "notile.glsl"

// Couleurs de base du fragment (calculees une fois dans main()).
vec3 gAlbedo = vec3(1.0);
vec3 gSpecTex = vec3(0.0);

vec4 mapDiffuse(vec2 uv) {
    return texNoTile(material.diffuse, uv) * vec4(surfaceTint(FragPos), 1.0);
}

// La brique est un matériau mat : pas de carte spéculaire, on utilise une
// intensité fixe faible au lieu du champ sampler specular.
const float SPECULAR_STRENGTH = 0.08;

// function prototypes
vec3 calcDirLight(DirLight light, vec3 normal, vec3 viewDir);
vec3 calcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir);
vec3 calcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir);

void main()
{
    gDdx = dFdx(TexCoords);
    gDdy = dFdy(TexCoords);
    gAlbedo = mapDiffuse(TexCoords).rgb;
    // ── Normal mapping ───────────────────────────────────────────────────
    // TBN reconstruit par derivees ecran (Mikkelsen, "Bump Mapping
    // Unparametrized Surfaces") : Vertex n'a pas de tangentes, et les murs
    // peuvent etre orientes dans n'importe quel sens (maisons tournees par
    // les includes du JSON, pignons...). La normale geometrique sortante +
    // les derivees de la position monde et des UV reconstituent le repere
    // tangent : la normal map s'applique correctement quelle que soit
    // l'orientation de la face (avant, elle etait utilisee telle quelle en
    // repere monde : relief correct uniquement sur les faces +Z).
    vec3 mapN = texNoTile(material.normal, TexCoords).rgb * 2.0 - 1.0;
    // brickwall_normal.jpg : Y vers le haut (convention OpenGL). Si le relief
    // semble creux (lumieres inversees), inverser la composante verte.
    vec3 N = normalize(Normal);
    vec3 dp1 = dFdx(FragPos);
    vec3 dp2 = dFdy(FragPos);
    vec2 duv1 = dFdx(TexCoords);
    vec2 duv2 = dFdy(TexCoords);
    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    float invmax = inversesqrt(max(dot(T, T), dot(B, B)));
    vec3 norm = normalize(mat3(T * invmax, B * invmax, N) * mapN);

    // properties
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

    FragColor = vec4(applyFog(result, length(viewPos - FragPos)), 1.0);
}

// calculates the color when using a directional light.
vec3 calcDirLight(DirLight light, vec3 normal, vec3 viewDir)
{
    vec3 lightDir = normalize(-light.direction);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    // combine results
    vec3 ambient = light.ambient * gAlbedo;
    vec3 diffuse = light.diffuse * diff * gAlbedo;
    vec3 specular = light.specular * spec * vec3(SPECULAR_STRENGTH);
    return (ambient + diffuse + specular);
}

// calculates the color when using a point light.
vec3 calcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    // attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    // combine results
    vec3 ambient = light.ambient * gAlbedo;
    vec3 diffuse = light.diffuse * diff * gAlbedo;
    vec3 specular = light.specular * spec * vec3(SPECULAR_STRENGTH);
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    return (ambient + diffuse + specular);
}

// calculates the color when using a spot light.
vec3 calcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    // attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    // spotlight intensity
    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
    // combine results
    vec3 ambient = light.ambient * gAlbedo;
    vec3 diffuse = light.diffuse * diff * gAlbedo;
    vec3 specular = light.specular * spec * vec3(SPECULAR_STRENGTH);
    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;
    return (ambient + diffuse + specular);
}