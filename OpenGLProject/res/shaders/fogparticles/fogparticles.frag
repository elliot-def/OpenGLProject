#version 330 core
in vec2 vUV;
in vec4 vColor;
in float vDist;
out vec4 FragColor;

uniform sampler2D uTex;   // atlas 2x2 de cercles blancs/gris (FogParticles)

void main()
{
    vec4 t = texture(uTex, vUV);
    // Estompe les particules collees a la camera (pas de voile brutal devant le visage).
    float nearFade = smoothstep(0.6, 3.5, vDist);
    float a = t.a * vColor.a * nearFade;
    if (a < 0.002) discard;
    FragColor = vec4(t.rgb * vColor.rgb, a);
}
