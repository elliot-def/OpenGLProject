#version 330 core
layout (location = 0) in vec3 aPos;

// Attributs par-instance : mat4 model (locations 4-7) + vec3 color (location 8).
layout (location = 4) in mat4 aInstanceModel;
layout (location = 8) in vec3 aInstanceColor;

out vec3 vLightColor;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    vLightColor = aInstanceColor;
    gl_Position = projection * view * aInstanceModel * vec4(aPos, 1.0);
}
