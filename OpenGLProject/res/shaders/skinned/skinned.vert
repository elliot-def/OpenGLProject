#version 330 core

// Attributs finaux du Vertex (cf. Vertex.h) :
//   0 = aPos         (vec3) — toujours actif
//   1 = aNormal      (vec3) — optionnel
//   2 = aColor       (vec3) — optionnel (non utilise ici)
//   3 = aTexCoords   (vec2) — optionnel
//   4 = aBoneIDs[4]  (int,  glVertexAttribIPointer) — skinning
//   5 = aWeights[4]  (float) — skinning
//
// Convention identique a res/shaders/model/model.vert, plus l'extension
// skinning 4+5. Le mesh non-rig (Cube, backpack) laisse les boneIDs a 0 et
// les weights a 0 : le garde-fou wsum<=0 ci-dessous bascule sur une
// transformation identite -> pas besoin d'un deuxieme shader "non-skinned".
layout (location = 0) in vec3  aPos;
layout (location = 1) in vec3  aNormal;
layout (location = 3) in vec2  aTexCoords;
layout (location = 4) in ivec4 aBoneIDs;
layout (location = 5) in vec4  aWeights;

out vec2 TexCoords;
out vec3 FragPos;
out vec3 Normal;

// Matrices espace monde : envoyees par Shader::setupMatrices().
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// Palette de bones. Taille 100 (cf. MAX_BONES dans ArmsRenderer.cpp), uploadee
// via Shader::setMat4Array("uBoneMatrices", padded, 100). Le slot i contient
// globalTransform * offsetMatrix, calcule par Animator::updateNodeHierarchy
// (cf. OpenGLProject/Animator.cpp).
const int MAX_BONE_INFLUENCE = 4;
const int MAX_BONES          = 100;
uniform mat4 uBoneMatrices[MAX_BONES];

void main() {
    // Blend pondere des transformations bone-espace appliquées au vertex
    // courant. Pour chaque influence non-nulle (w > 0) :
    //   pos += bone * vec4(localPos, 1) * w
    //   nrm += mat3(bone) * localNormal * w (la rotation seule du bone)
    vec4 totalPos = vec4(0.0);
    vec3 totalNor = vec3(0.0);
    float wsum    = 0.0;

    for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
        float w = aWeights[i];
        if (w > 0.0) {
            int id = aBoneIDs[i];
            mat4 bone = uBoneMatrices[id];
            totalPos += bone * vec4(aPos, 1.0) * w;
            totalNor += mat3(bone) * aNormal * w;
            wsum += w;
        }
    }

    // Garde-fou : mesh non-rig (tous weights a 0) -> transformation identite.
    // Assimp remplit toujours les bone slots quand le mesh est rig, donc
    // wsum == 0 implique un mesh classique (Cube, backpack, etc.).
    if (wsum <= 0.0) {
        totalPos = vec4(aPos, 1.0);
        totalNor = aNormal;
    } else {
        // Normalisation : compense la normalisation faite cote CPU par
        // Model::extractBoneDataFromMesh (Assimp normalise aussi), mais en
        // cas de derive (somme != 1) on evite les artefacts de scale.
        totalPos = vec4(totalPos.xyz / wsum, 1.0);
        totalNor = normalize(totalNor / wsum);
    }

    // Passage en espace monde puis projection.
    FragPos   = vec3(model * totalPos);
    // Normale : mat3(transpose(inverse(model))) — correct pour les scales
    // non-uniformes (mat3(model) ne suffit pas). Cout CPU negligeable sur
    // les avant-bras (low poly).
    Normal    = mat3(transpose(inverse(model))) * totalNor;
    TexCoords = aTexCoords;

    gl_Position = projection * view * vec4(FragPos, 1.0);
}
