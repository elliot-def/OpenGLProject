#version 330 core

// ─────────────────────────────────────────────────────────────────────────────
// skinned.frag : fragment shader pour les modeles rigges.
//
// Reprend la convention d'eclairage diffuse+specular du model traditionnel.
// Le reste (multiple lights, shadow mapping, PBR) pourra etre etendu en
// remplacant cette implementation par celle de model.frag.
// ─────────────────────────────────────────────────────────────────────────────

in vec3 FragPos;
in vec3 Normal;
in vec3 VertexColor;
in vec2 TexCoords;

out vec4 FragColor;

uniform sampler2D texture_diffuse;   // unit 0
uniform sampler2D texture_specular;  // unit 1
uniform int        hasSpecularMap;
uniform vec3       viewPos;

uniform int numLights;
uniform vec3 lightPositions[16];
uniform vec3 lightColors  [16];
uniform float lightIntensities[16];

void main() {
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    vec3 diffuseAlbedo = texture(texture_diffuse, TexCoords).rgb;

    vec3 ambient  = 0.15 * diffuseAlbedo;
    vec3 lighting = vec3(0.0);

    for (int i = 0; i < numLights && i < 16; ++i) {
        vec3 lightDir = normalize(lightPositions[i] - FragPos);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = lightIntensities[i] * lightColors[i] * diff * diffuseAlbedo;

        vec3 spec = vec3(0.0);
        if (hasSpecularMap == 1) {
            vec3 reflectDir = reflect(-lightDir, norm);
            float specFactor = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
            vec3 specColor = texture(texture_specular, TexCoords).rgb;
            spec = lightIntensities[i] * lightColors[i] * specFactor * specColor;
        }

        lighting += diffuse + spec;
    }

    FragColor = vec4(ambient + lighting, 1.0);
}
