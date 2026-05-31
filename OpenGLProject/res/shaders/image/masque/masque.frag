#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D image;
uniform float opacity;
uniform vec3 color;

void main() {
    vec4 texColor = texture(image, TexCoord);
    FragColor = vec4(color, texColor.a * opacity);
}