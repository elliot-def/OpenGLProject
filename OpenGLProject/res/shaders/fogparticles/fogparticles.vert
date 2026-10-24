#version 330 core
// Particules de brume : quads billboard deja en repere monde.
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;
layout (location = 2) in vec4 aColor;   // rgb = gris, a = opacite (enveloppe de vie incluse)

out vec2 vUV;
out vec4 vColor;
out float vDist;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 viewPos;

void main()
{
    vUV = aUV;
    vColor = aColor;
    vDist = length(viewPos - aPos);
    gl_Position = projection * view * vec4(aPos, 1.0);
}
