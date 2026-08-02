#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <assimp/scene.h>
#include <vector>
#include <string>
#include <unordered_map>

#include "SkinningData.h"
#include "Model.h"

class Animator {
public:
    Animator() = default;

    // Initialise l'animator avec le modèle (pour accéder au scene et boneInfoMap)
    void setup(Model* model);

    // Joue une animation par son index dans scene->mAnimations[]
    void playAnimation(unsigned int animIndex, bool loop = true);

    // Met à jour l'animation (à appeler chaque frame)
    void update(float deltaTime);

    // Retourne les matrices finales des bones à envoyer au shader
    const std::vector<glm::mat4>& getFinalBoneMatrices() const { return m_finalBoneMatrices; }

    // Vérifie si une animation est en cours
    bool isPlaying() const { return m_currentAnimation != nullptr; }

    // Vérifie si une animation NON-loop est arrivée à son terme (pour
    // enchaîner sur l'idle après un tir, par exemple)
    bool isFinished() const {
        return m_currentAnimation != nullptr && !m_loop &&
               m_currentTime >= static_cast<float>(m_currentAnimation->mDuration);
    }

    // Retourne le nom de l'animation en cours
    const std::string& getCurrentAnimationName() const { return m_currentAnimName; }

    // Applique un offset local supplémentaire à un bone par son nom (ex:
    // décaler un bras). L'offset est composé avec la transformation du bone
    // à chaque frame, entre la transformation globale et l'offsetMatrix.
    // Passer glm::mat4(1.0f) retire l'offset du bone.
    void addBoneOffset(const std::string& boneName, const glm::mat4& offset) {
        m_boneOffsets[boneName] = offset;
    }

    // Supprime tous les offsets manuels
    void clearBoneOffsets() { m_boneOffsets.clear(); }

private:
    Model* m_model = nullptr;
    const aiScene* m_scene = nullptr;
    const aiAnimation* m_currentAnimation = nullptr;
    std::string m_currentAnimName;
    float m_currentTime = 0.0f;
    float m_ticksPerSecond = 30.0f;
    bool m_loop = true;

    std::vector<glm::mat4> m_finalBoneMatrices;

    // Offsets manuels optionnels appliqués par nom de bone (voir addBoneOffset)
    std::unordered_map<std::string, glm::mat4> m_boneOffsets;

    // Cache des canaux d'animation par nom de nœud (reconstruit à chaque
    // playAnimation) : évite la recherche linéaire dans mNumChannels à chaque
    // frame, et permet d'animer TOUS les nœuds canalisés (pas seulement les
    // bones) — un conteneur figé désynchronise sa hiérarchie d'enfants.
    std::unordered_map<std::string, const aiNodeAnim*> m_channelMap;

    // Assimp 5.4.3 corrompt les clés de rotation à l'import (1 valide sur 4,
    // le reste = bruit). Filtre les clés valides (temps fini dans [0, durée],
    // quaternion unitaire, monotonie) et les compacte. À appeler dans
    // playAnimation, avant le remplissage de m_channelMap.
    void repairRotationKeys(const aiAnimation* anim);

    // Trouve la transformation d'un nœud à un instant donné dans l'animation.
    // node sert de fallback : si le canal n'a pas de clé pour un composant
    // (position/rotation), on utilise la transformation de repos (bind pose)
    // du nœud au lieu de (0,0,0)/identité — sinon les bones se collent au
    // centre du rig ou pivotent vers +Y (bras collés / pointant vers le haut).
    glm::mat4 interpolateNodeTransform(const aiNode* node, const aiNodeAnim* channel, float animTime) const;

    // Calcule récursivement les matrices globales des bones
    void computeBoneTransform(const aiNode* node, const glm::mat4& parentTransform, float animTime);

    // Helper : interpolation de translation (defaultValue = bind pose du nœud,
    // utilisée si le canal n'a pas de clé de position)
    glm::vec3 interpolateTranslation(float animTime, const aiNodeAnim* channel,
                                     const glm::vec3& defaultValue) const;

    // Helper : interpolation de rotation (defaultValue = bind pose du nœud,
    // utilisée si le canal n'a pas de clé de rotation)
    glm::quat interpolateRotation(float animTime, const aiNodeAnim* channel,
                                  const glm::quat& defaultValue) const;

    // Helper : interpolation de scale
    glm::vec3 interpolateScale(float animTime, const aiNodeAnim* channel) const;

    // Trouve l'index de la keyframe pour un temps donné
    unsigned int findKeyIndex(float animTime, const aiVectorKey* keys, unsigned int numKeys) const;
    unsigned int findKeyIndex(float animTime, const aiQuatKey* keys, unsigned int numKeys) const;
};
