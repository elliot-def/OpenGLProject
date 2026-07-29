#version 330 core

// ─────────────────────────────────────────────────────────────────────────────
// skinned.vert : vertex shader pour les modeles rigges (skinned).
//
// Convention uniformes (alignee avec model.vert) :
//   model, view, projection : MVP standard
//   viewPos                  : position camera (calculee cote C++)
//   uBoneMatrices[MAX_BONES] : palette de matrices finals (offset * global).
//
// Convention attribs (loc):
//   0 vec3 aPos
//   1 vec3 aNormal
//   2 vec3 aColor (optionnel)
//   3 vec2 aTexCoords
//   4 ivec4 aBoneIDs   (optionnel — skinned only)
//   5 vec4  aWeights   (optionnel — skinned only)
//
// Si weights somme a 0 (mesh non-skinne), on retombe sur le model brut
// (multiplication par identite) pour eviter tout deplacement parasite.
// ─────────────────────────────────────────────────────────────────────────────

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in vec2 aTexCoords;
layout(location = 4) in ivec4 aBoneIDs;
layout(location = 5) in vec4  aWeights;

const int MAX_BONES = 100;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 uBoneMatrices[MAX_BONES];

out vec3 FragPos;
out vec3 Normal;
out vec3 VertexColor;
out vec2 TexCoords;

void main() {
    // 1. Application du skinning (matrices finals par bone, ponderees par
    //    influence). Si le mesh n'est pas rigge (weights=[0,0,0,0]), le total
    //    est identite -> pas de transformation sur la position.
    mat4 boneTransform = mat4(1.0);
    float wsum = aWeights.x + aWeights.y + aWeights.z + aWeights.w;
    if (wsum > 1e-5) {
        mat4 m0 = uBoneMatrices[aBoneIDs[0]] * aWeights[0];
        mat4 m1 = uBoneMatrices[aBoneIDs[1]] * aWeights[1];
        mat4 m2 = uBoneMatrices[aBoneIDs[2]] * aWeights[2];
        mat4 m3 = uBoneMatrices[aBoneIDs[3]] * aWeights[3];
        boneTransform = m0 + m1 + m2 + m3;
    }

    vec4 posModel = boneTransform * vec4(aPos, 1.0);

    // 2. Normale transformee uniquement par la partie rotation/scale de
    //    boneTransform (le bone "L" est scale non-uniforme possible -> on
    //    utilise la transposee de l'inverse pour eviter les mauvaises
    //    normales sur mesh rigides).
    mat3 normalMatrix = mat3(transpose(inverse(boneTransform)));
    vec3 normalModel = normalMatrix * aNormal;

    // 3. Position finale.
    vec4 worldPos = model * posModel;
    FragPos      = vec3(worldPos);
    Normal       = mat3(model) * normalModel;
    VertexColor  = aColor;
    TexCoords    = aTexCoords;

    gl_Position = projection * view * worldPos;
    //gl_Position = projection * view * model * vec4(aPos, 1.0);
}
