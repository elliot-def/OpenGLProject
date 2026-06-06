#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;

uniform sampler2D texture_diffuse;
uniform sampler2D texture_specular;

uniform vec3 viewPos;

// ===== DirLight =====
struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
uniform DirLight dirLight;

// ===== PointLights =====
struct PointLight {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
};
#define MAX_POINT_LIGHTS 10
uniform PointLight lightSources[MAX_POINT_LIGHTS];
uniform int numberLightSources;

// ===== Spotlight =====
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
uniform SpotLight spotLight; // nom identique à Spotlight::applyToShader

// ===== Fonctions =====
vec3 calcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 diffuseTex, vec3 specularTex) {
    vec3 lightDir   = normalize(-light.direction);
    float diff      = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec      = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);

    vec3 ambient  = light.ambient  * diffuseTex;
    vec3 diffuse  = light.diffuse  * diff * diffuseTex;
    vec3 specular = light.specular * spec * specularTex;
    return ambient + diffuse + specular;
}

vec3 calcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 diffuseTex, vec3 specularTex) {
    vec3 lightDir   = normalize(light.position - fragPos);
    float diff      = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec      = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);

    float distance    = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);

    vec3 ambient  = light.ambient  * diffuseTex  * attenuation;
    vec3 diffuse  = light.diffuse  * diff * diffuseTex  * attenuation;
    vec3 specular = light.specular * spec * specularTex * attenuation;
    return ambient + diffuse + specular;
}

vec3 calcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 diffuseTex, vec3 specularTex) {
    // Si diffuse est à 0 (flashlight désactivée), on retourne juste l'ambient
    // qui sera aussi à 0 puisque applyToShader met tout à 0 quand désactivée
    vec3 lightDir   = normalize(light.position - fragPos);
    float diff      = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec      = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);

    float distance    = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);

    float theta     = dot(lightDir, normalize(-light.direction));
    float epsilon   = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

    vec3 ambient  = light.ambient  * diffuseTex  * attenuation;
    vec3 diffuse  = light.diffuse  * diff * diffuseTex  * attenuation * intensity;
    vec3 specular = light.specular * spec * specularTex * attenuation * intensity;
    return ambient + diffuse + specular;
}


void main() {
   
    vec3 norm        = normalize(Normal);
    vec3 viewDir     = normalize(viewPos - FragPos);
    vec3 diffuseTex  = texture(texture_diffuse,  TexCoords).rgb;
    vec3 specularTex = texture(texture_specular, TexCoords).rgb;

    vec3 result = vec3(0.0);
    result += calcDirLight(dirLight, norm, viewDir, diffuseTex, specularTex);

    for (int i = 0; i < numberLightSources; i++) {
        result += calcPointLight(lightSources[i], norm, FragPos, viewDir, diffuseTex, specularTex);
    }

    result += calcSpotLight(spotLight, norm, FragPos, viewDir, diffuseTex, specularTex);

    FragColor = vec4(result, 1.0);
}