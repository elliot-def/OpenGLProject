#version 330 core

// ─── door.vert : porte interactive (rectangle pivotant) ───
// Meme principe que wall.vert : matrice model classique (pas d'instanciation),
// normales transformees par la transposee de l'inverse (rotation uniforme).

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 3) in vec2 aTexCoords;

out vec3 FragPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;

    gl_Position = projection * view * vec4(FragPos, 1.0);
}
