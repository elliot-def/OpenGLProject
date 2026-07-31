#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
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

    // Retourne le nom de l'animation en cours
    const std::string& getCurrentAnimationName() const { return m_currentAnimName; }

private:
    Model* m_model = nullptr;
    const aiScene* m_scene = nullptr;
    const aiAnimation* m_currentAnimation = nullptr;
    std::string m_currentAnimName;
    float m_currentTime = 0.0f;
    float m_ticksPerSecond = 30.0f;
    bool m_loop = true;

    std::vector<glm::mat4> m_finalBoneMatrices;

    // Trouve la transformation d'un nœud à un instant donné dans l'animation
    glm::mat4 interpolateNodeTransform(const aiNodeAnim* channel, float animTime) const;

    // Calcule récursivement les matrices globales des bones
    void computeBoneTransform(const aiNode* node, const glm::mat4& parentTransform, float animTime);

    // Helper : interpolation de translation
    glm::vec3 interpolateTranslation(float animTime, const aiNodeAnim* channel) const;

    // Helper : interpolation de rotation
    glm::quat interpolateRotation(float animTime, const aiNodeAnim* channel) const;

    // Helper : interpolation de scale
    glm::vec3 interpolateScale(float animTime, const aiNodeAnim* channel) const;

    // Trouve l'index de la keyframe pour un temps donné
    unsigned int findKeyIndex(float animTime, const aiVectorKey* keys, unsigned int numKeys) const;
    unsigned int findKeyIndex(float animTime, const aiQuatKey* keys, unsigned int numKeys) const;
};
