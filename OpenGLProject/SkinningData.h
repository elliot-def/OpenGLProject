#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <assimp/scene.h>

inline constexpr int MAX_BONE_INFLUENCE = 4;
inline constexpr int MAX_BONES = 128; // marge confortable, le vrai compte vient du fichier GLB au runtime

// Taille du tableau uniform `uBoneMatrices` cote GPU
// (res/shaders/skinned/skinned.vert : `const int MAX_BONES = 52;`).
// Le count passe a setMat4Array() ne doit JAMAIS le depasser, sinon on
// ecrit hors du tableau uniform (comportement indefini selon le driver).
// Si la taille du tableau shader change, mettez ce define a jour.
inline constexpr int SHADER_MAX_BONES = 52;

struct BoneInfo {
    int      id = -1;
    glm::mat4 offsetMatrix{ 1.0f };
};

inline glm::mat4 aiMatrixToGlm(const aiMatrix4x4& ai) {
    // Assimp stocke en row-major : ai[i][j] = rangée i, colonne j
    // GLM  stocke en column-major : glm[col][row]
    // On lit les COLONNES d'Assimp (haut→bas) pour remplir les colonnes GLM
    return glm::mat4(
        ai.a1, ai.b1, ai.c1, ai.d1,  // colonne 0
        ai.a2, ai.b2, ai.c2, ai.d2,  // colonne 1
        ai.a3, ai.b3, ai.c3, ai.d3,  // colonne 2
        ai.a4, ai.b4, ai.c4, ai.d4   // colonne 3
    );
}
