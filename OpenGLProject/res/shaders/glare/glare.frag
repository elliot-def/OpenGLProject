#version 330 core
// Eblouissement du soleil : halo, rayons radiaux, reflets d'objectif et voile
// blanc quand on le regarde en face. Melange "screen" (voir Scene::drawSunGlare).
in vec2 ndc;
out vec4 FragColor;

uniform vec2  sunNdc;      // position ecran du soleil (NDC)
uniform float aspect;      // largeur / hauteur
uniform float intensity;   // 0..1 : halo + rayons (large cone de regard)
uniform float dazzle;      // 0..1 : voile d'eblouissement (regard quasi centre)
uniform float time;

const vec3 WARM = vec3(1.0, 0.88, 0.55);

void main() {
    vec2 p = (ndc - sunNdc) * vec2(aspect, 1.0);
    float r = length(p);
    float ang = atan(p.y, p.x);

    float core = exp(-r * 7.0) * 1.6;
    float halo = exp(-r * 1.7) * 0.55;

    // Rayons : deux harmoniques de frequences premieres qui tournent lentement.
    float s1 = sin(ang * 17.0 + time * 0.15);
    float s2 = sin(ang * 29.0 - time * 0.11 + 1.7);
    float s3 = sin(ang * 7.0  + time * 0.05);
    float rays = pow(clamp(abs(s1 * s2) + 0.35 * abs(s3), 0.0, 1.0), 3.0) * exp(-r * 1.1) * 0.9;

    // Reflets d'objectif : sur la droite sun -> centre de l'ecran, a l'oppose.
    vec3 ghosts = vec3(0.0);
    for (int i = 0; i < 3; ++i) {
        float k = -0.35 - 0.45 * float(i);
        vec2 gp = (ndc - sunNdc * k) * vec2(aspect, 1.0);
        float d = length(gp);
        float size = 0.10 + 0.05 * float(i);
        float ring = (1.0 - smoothstep(size * 0.8, size, d)) * smoothstep(size * 0.5, size, d);
        ghosts += vec3(0.5 + 0.15 * float(i), 0.7, 1.0 - 0.2 * float(i)) * (ring + 0.25 * (1.0 - smoothstep(0.0, size, d))) * 0.25;
    }

    vec3 col = WARM * (core + halo + rays) * intensity + ghosts * intensity;

    // Voile : tout l'ecran blanchit, plus fort vers le soleil.
    float veil = dazzle * (0.45 + 0.55 * exp(-r * 0.9));
    col += vec3(1.0, 0.96, 0.85) * veil;

    // Garde-fou : un NaN avec le blend "screen" noircirait tout l'ecran.
    if (any(isnan(col)) || any(isinf(col))) col = vec3(0.0);
    FragColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}
