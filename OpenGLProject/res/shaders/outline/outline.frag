#version 330 core
out vec4 FragColor;

uniform vec3 uOutlineColor;
// Opacite du contour (1 par defaut = opaque). Seule la porte (Door::draw) la
// definite pour animer le fondu/pulse de son surlignage ; les autres
// utilisateurs de ce shader ne la definissent pas et restent opaques.
uniform float uOutlineAlpha = 1.0;

void main() {
    // Couleur plate, pas de lumière, pas de texture. L'outline est une silhouette.
    FragColor = vec4(uOutlineColor, uOutlineAlpha);
}
