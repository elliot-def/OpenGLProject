#version 330 core

// ─── wallhack.vert : silhouette X-ray des joueurs distants ───
// Même layout d'attributs que skinned.vert (positions + boneIDs/weights),
// mais on ne calcule QUE la position clip-space (pas de Normale/FragPos :
// le fragment shader sort une couleur unie, pas de lumières).

layout (location = 0) in vec3 aPos;
layout (location = 4) in ivec4 aBoneIDs;
layout (location = 5) in vec4 aWeights;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

const int MAX_BONES = 100;
uniform mat4 uBoneMatrices[MAX_BONES];

void main()
{
    // Combinaison pondérée des matrices d'os (identique à skinned.vert)
    mat4 boneTransform = mat4(0.0);
    for (int i = 0; i < 4; i++) {
        if (aBoneIDs[i] >= 0 && aBoneIDs[i] < MAX_BONES) {
            boneTransform += uBoneMatrices[aBoneIDs[i]] * aWeights[i];
        }
    }
    float wsum = aWeights.x + aWeights.y + aWeights.z + aWeights.w;
    if (wsum < 0.001) {
        boneTransform = mat4(1.0);
    }

    vec4 skinnedPos = boneTransform * vec4(aPos, 1.0);
    gl_Position = projection * view * model * skinnedPos;
}
