#version 330 core

// Couleur unie opaque (alpha=1) -- convention : le C++ garde GL_BLEND
// desactive pendant l'outline pass, donc on n'a PAS besoin de pre-multiplied
// alpha ici. Si tu modifies ce frag pour ajouter de la transparence,
// verifie aussi que call site active GL_BLEND avec le bon glBlendFunc.
out vec4 FragColor;

uniform vec3 uOutlineColor;

void main() {
    FragColor = vec4(uOutlineColor, 1.0);
}
