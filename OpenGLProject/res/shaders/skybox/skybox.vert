#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 vPos; // position locale = direction d'echantillonnage du cubemap

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    vPos = aPos;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
