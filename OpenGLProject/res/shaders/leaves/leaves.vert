#version 330 core
// Feuilles au sol : geometrie deja en repere monde (pas de matrice model).
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 3) in vec2 aTexCoords;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = aPos;
    Normal = aNormal;
    TexCoords = aTexCoords;
    gl_Position = projection * view * vec4(aPos, 1.0);
}
