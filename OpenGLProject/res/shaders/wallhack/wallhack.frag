#version 330 core

// ─── wallhack.frag : couleur unie (silhouette X-ray) ───
// Aucune texture, aucune lumière : on sort une couleur bleue semi-transparente.
// L'alpha est piloté par un uniform (blending activé côté C++).

out vec4 FragColor;

uniform vec3  uColor = vec3(0.20, 0.50, 1.00);  // bleu
uniform float uAlpha = 0.45;                      // translucide

void main()
{
    FragColor = vec4(uColor, uAlpha);
}
