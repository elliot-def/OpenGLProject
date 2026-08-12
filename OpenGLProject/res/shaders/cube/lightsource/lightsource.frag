#version 330 core
out vec4 FragColor;

in vec3 vLightColor;

void main()
{
    FragColor = vec4(vLightColor, 1.0); // couleur de la lumiere par instance
}
