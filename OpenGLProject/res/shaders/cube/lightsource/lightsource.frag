#version 330 core
out vec4 FragColor;

in vec3 vLightColor;
in vec3 vWorldPos;
uniform vec3 viewPos;

#include "fog.glsl"

void main()
{
    FragColor = vec4(applyFog(vLightColor, length(viewPos - vWorldPos)), 1.0); // couleur par instance, brumisee
}
