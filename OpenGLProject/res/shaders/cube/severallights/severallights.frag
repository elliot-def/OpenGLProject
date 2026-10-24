#version 330 core
out vec4 FragColor;

struct Material {
    sampler2D diffuse;
    sampler2D specular;
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

uniform int numberLightSources;    // Nombre r�el de lumi�res actives

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

// uPlainTexture = 1 : texture de type palette (arbres, UV constants par face) :
// un decalage UV aleatoire y piocherait une couleur au hasard (tronc vert) ->
// echantillonnage direct, teinte monde attenuee.
uniform float uPlainTexture;
uniform float uAlpha;     // > 0 : opacite (vitres des modeles OBJ) ; 0 = opaque
uniform vec3 uTreeTint;   // delta de couleur par arbre (MapPart::tint)

// Couleurs de base du fragment (calculees une fois dans main()).
vec3 gAlbedo = vec3(1.0);
vec3 gSpecTex = vec3(0.0);

vec4 mapDiffuse(vec2 uv) {
    if (uPlainTexture > 0.5) {
        return textureGrad(material.diffuse, uv, gDdx, gDdy) * vec4(mix(vec3(1.0), surfaceTint(FragPos), 0.15) * uTreeTint, 1.0);
    }
    return texNoTile(material.diffuse, uv) * vec4(surfaceTint(FragPos), 1.0);
}

// function prototypes
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir);
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir);

void main()
{
    gDdx = dFdx(TexCoords);
    gDdy = dFdy(TexCoords);
    gAlbedo = mapDiffuse(TexCoords).rgb;
    gSpecTex = texNoTile(material.specular, TexCoords).rgb;
    // properties
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    
    // phase 1: directional lighting
    vec3 result = CalcDirLight(dirLight, norm, viewDir);
    
    // phase 2: point lights (lumiere interieure bloquee hors de la piece)
    for(int i = 0; i < numberLightSources; i++) {
        if (!roomAllows(i, FragPos)) continue;
        result += CalcPointLight(lightSources[i], norm, FragPos, viewDir);
    }
    
    // phase 3: spot light
    result += CalcSpotLight(spotLight, norm, FragPos, viewDir);
    result += CalcSpotLight(sunSpot, norm, FragPos, viewDir);
    
    FragColor = vec4(applyFog(result, length(viewPos - FragPos)), uAlpha > 0.0 ? uAlpha : 1.0);
}

// calculates the color when using a directional light.
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir)
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
    vec3 specular = light.specular * spec * gSpecTex;
    return (ambient + diffuse + specular);
}

// calculates the color when using a point light.
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
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
    vec3 specular = light.specular * spec * gSpecTex;
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    return (ambient + diffuse + specular);
}

// calculates the color when using a spot light.
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
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
    vec3 specular = light.specular * spec * gSpecTex;
    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;
    return (ambient + diffuse + specular);
}