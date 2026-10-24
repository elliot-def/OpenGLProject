#version 330 core
// Feuilles au sol : decoupe alpha + eclairage par la lumiere directionnelle.
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
out vec4 FragColor;

struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform DirLight dirLight;
uniform sampler2D uTex;
uniform vec3 viewPos;

#include "fog.glsl"

void main()
{
    vec4 t = texture(uTex, TexCoords);
    if (t.a < 0.5) discard;

    vec3 n = normalize(Normal);
    float diff = max(dot(n, normalize(-dirLight.direction)), 0.0);
    vec3 light = dirLight.ambient + dirLight.diffuse * diff;
    FragColor = vec4(applyFog(t.rgb * min(light, vec3(1.3)), length(viewPos - FragPos)), 1.0);
}
