// ── Anti-repetition des textures ─────────────────────────────────────────────
// 1) texNoTile : chaque cellule UV recoit un decalage aleatoire (hash), et les
//    4 cellules voisines sont fondues -> plus de motif repete visible.
//    Decalage seul (ni miroir ni rotation) : valable aussi pour la normal map.
// 2) surfaceTint : bruit de valeur en repere monde (grande + petite echelle,
//    3 plans) qui module la luminosite/teinte -> chaque surface est unique.
// Derivees calculees UNE fois dans main() : textureGrad reste correct dans les
// boucles de lumieres (flux non uniforme).
vec2 gDdx = vec2(0.0);
vec2 gDdy = vec2(0.0);

vec4 hash4(vec2 p) {
    return fract(sin(vec4(1.0 + dot(p, vec2(37.0, 17.0)),
                          2.0 + dot(p, vec2(11.0, 47.0)),
                          3.0 + dot(p, vec2(41.0, 29.0)),
                          4.0 + dot(p, vec2(23.0, 31.0)))) * 103.0);
}

vec4 texNoTile(sampler2D samp, vec2 uv) {
    vec2 iuv = floor(uv);
    vec2 fuv = fract(uv);
    vec4 va = textureGrad(samp, uv + hash4(iuv).zw,                gDdx, gDdy);
    vec4 vb = textureGrad(samp, uv + hash4(iuv + vec2(1.0, 0.0)).zw, gDdx, gDdy);
    vec4 vc = textureGrad(samp, uv + hash4(iuv + vec2(0.0, 1.0)).zw, gDdx, gDdy);
    vec4 vd = textureGrad(samp, uv + hash4(iuv + vec2(1.0, 1.0)).zw, gDdx, gDdy);
    vec2 b = smoothstep(0.25, 0.75, fuv);
    return mix(mix(va, vb, b.x), mix(vc, vd, b.x), b.y);
}

float valueNoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash4(i).x, hash4(i + vec2(1.0, 0.0)).x, f.x),
               mix(hash4(i + vec2(0.0, 1.0)).x, hash4(i + vec2(1.0, 1.0)).x, f.x), f.y);
}

float triNoise(vec3 p, float scale) {
    return (valueNoise(p.xz * scale) + valueNoise(p.xy * scale) + valueNoise(p.zy * scale)) / 3.0;
}

vec3 surfaceTint(vec3 p) {
    float large = triNoise(p, 0.22);          // taches de ~4 m
    float mid   = triNoise(p + 17.3, 0.9);    // ~1 m
    float fine  = triNoise(p + 41.7, 3.5);    // ~30 cm
    float v = mix(0.55, 1.45, large) * mix(0.8, 1.2, mid) * mix(0.9, 1.1, fine);
    float h = mid - 0.5;
    return vec3(v * (1.0 + 0.10 * h), v, v * (1.0 - 0.14 * h));   // legere derive de teinte
}
