#version 330 core

// ─── skinned.vert : vertex shader pour modèles riggés ───
// layout 0 = position, 1 = normal, 2 = color, 3 = texcoord,
// layout 4 = ivec4 boneIDs, 5 = vec4 weights

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;
layout (location = 3) in vec2 aTexCoords;
layout (location = 4) in ivec4 aBoneIDs;
layout (location = 5) in vec4 aWeights;

out vec3 FragPos;
out vec3 Normal;
out vec3 Color;
out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

const int MAX_BONES = 100;
uniform mat4 uBoneMatrices[MAX_BONES];

void main()
{
    // Calculer la matrice skinning comme combinaison pondérée
    mat4 boneTransform = mat4(0.0);
    for (int i = 0; i < 4; i++) {
        if (aBoneIDs[i] >= 0 && aBoneIDs[i] < MAX_BONES) {
            boneTransform += uBoneMatrices[aBoneIDs[i]] * aWeights[i];
        }
    }
    // Si aucun poids (somme nulle), utiliser l'identité
    float wsum = aWeights.x + aWeights.y + aWeights.z + aWeights.w;
    if (wsum < 0.001) {
        boneTransform = mat4(1.0);
    }

    vec4 skinnedPos = boneTransform * vec4(aPos, 1.0);
    vec4 skinnedNormal = boneTransform * vec4(aNormal, 0.0);

    gl_Position = projection * view * model * skinnedPos;
    FragPos = vec3(model * skinnedPos);
    Normal = mat3(transpose(inverse(model))) * vec3(skinnedNormal);
    Color = aColor;
    TexCoords = aTexCoords;
}
