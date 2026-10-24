// Brouillard exponentiel au carre (LightManager::setFog) : limite la vue.
uniform vec3  fogColor;
uniform float fogDensity;
vec3 applyFog(vec3 c, float dist) {
    float f = 1.0 - exp(-pow(dist * fogDensity, 2.0));
    return mix(c, fogColor, clamp(f, 0.0, 1.0));
}
