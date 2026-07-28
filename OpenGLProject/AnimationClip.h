#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// AnimationClip : données d'une animation skinned
//
// Chaque AnimationChannel pilote une transformation d'un bone (identifié par
// son nom de noeud assimp). Une AnimationClip agrège plusieurs canals pour
// produire l'animation complète d'un squelette (idle, walk, jump, etc.).
//
// Les PositionKey/RotationKey/ScaleKey sont les timestamps + valeurs samplées
// extraites depuis aiNodeAnim. A l'update du Animator, on cherche l'intervalle
// [t0, t1] qui contient m_currentTime et on interpole selon le type
// (lerp pour translation/scale, slerp pour rotation).
//
// Blend : on garde une liste de clips pondérés (m_blendWeights) pour permettre
// l'interpolation entre deux états (par ex. Walk -> Jump). En simple playback,
// m_blendWeights contient un seul clip à poids 1.
// ─────────────────────────────────────────────────────────────────────────────

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

// Cle de position : timestamp + vecteur 3D
struct PositionKey {
    double     time = 0.0;
    glm::vec3  value{ 0.0f };
};

// Cle de rotation : timestamp + quaternion
struct RotationKey {
    double           time = 0.0;
    glm::quat        value{ 1.0f, 0.0f, 0.0f, 0.0f };
};

// Cle d'echelle : timestamp + vecteur 3D
struct ScaleKey {
    double     time = 0.0;
    glm::vec3  value{ 1.0f };
};

// Une channel = un bone pilote par cette animation (identifie par son nom de
// noeud assimp ; le Animator fait la resolution nom -> index une fois au setup).
struct AnimationChannel {
    std::string                 nodeName;
    std::vector<PositionKey>    positionKeys;
    std::vector<RotationKey>    rotationKeys;
    std::vector<ScaleKey>       scaleKeys;
};

// Clip = une animation discrete avec son poids de blend (pour crossfade)
struct AnimationClip {
    std::string                    name;
    double                         duration        = 0.0;
    double                         ticksPerSecond  = 1.0;
    std::vector<AnimationChannel>  channels;
    float                          blendWeight     = 1.0f; // poids pour crossfade
};
