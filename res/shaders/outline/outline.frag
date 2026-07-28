#version 330 core
out vec4 FragColor;

uniform vec3 uOutlineColor;

void main() {
    // Couleur plate, pas de lumière, pas de texture. L'outline est une silhouette.
    FragColor = vec4(uOutlineColor, 1.0);
}
