#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// SkinningData : structures pour le rendu skinned (Vertex bones + Anim)
//
// Gère la séparation entre :
//   - Vertex bone IDs + weights (jusqu'à MAX_BONE_INFLUENCE influences)
//   - BoneInfo : offset matrix (inverse bind pose) par bone
//   - aiMatrix4x4 -> glm::mat4 (assimp est row-major, GLM est column-major)
//
// NOTE : assimp range toutes ses matrices en row-major et les fournit via
// l'opérateur [] de type aiMatrix4x4. GLM range ses matrices en column-major.
// Si on copie byte-à-byte sans transposer, la matrice est transposée
// silencieusement. Cette fonction centralise la conversion correcte.
// ─────────────────────────────────────────────────────────────────────────────

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <assimp/scene.h>

inline constexpr int MAX_BONE_INFLUENCE = 4; // inline = external linkage en C++17, evite C5260 (linkage interne en contexte header).

// Informations globales d'un bone dans le squelette.
// `id` est l'index dans le BoneInfoMap racine (et dans uBoneMatrices[] côté shader).
// `offsetMatrix` est la matrice inverse-bind-pose (M_bone^-1 * M_global au bind),
// appliquée a chaque vertex skinne pour le ramener dans l'espace du bone au repos.
struct BoneInfo {
    int         id = -1;
    glm::mat4   offsetMatrix{ 1.0f };
};

// Convertit une matrice assimp (row-major) vers une matrice GLM (column-major)
// par transposition explicite. A utiliser pour TOUTE matrice issue de l'API
// assimp (aiMatrix4x4, aiNode::mTransformation, aiBone::mOffsetMatrix, etc.).
//
// Convention : la matrice ai est lue par ai[i][j] = rangée i, colonne j (row-major).
// GLM utilise mat[col][row] = colonne col, rangée row (column-major).
// Donc glm::mat4 m;  m[col][row] = ai[row][col];  -> transposition manuelle.
inline glm::mat4 aiMatrixToGlm(const aiMatrix4x4& ai) {
    return glm::mat4(
        ai.a1, ai.a2, ai.a3, ai.a4,
        ai.b1, ai.b2, ai.b3, ai.b4,
        ai.c1, ai.c2, ai.c3, ai.c4,
        ai.d1, ai.d2, ai.d3, ai.d4
    );
}
