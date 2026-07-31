#include "Animator.h"
#include <algorithm>
#include <iostream>

void Animator::setup(Model* model) {
    m_model = model;
    m_scene = model->getScene();
    m_finalBoneMatrices.resize(MAX_BONES, glm::mat4(1.0f));
}

void Animator::playAnimation(unsigned int animIndex, bool loop) {
    if (!m_scene || animIndex >= m_scene->mNumAnimations) {
        std::cerr << "[Animator] Animation index " << animIndex << " invalide" << std::endl;
        return;
    }
    m_currentAnimation = m_scene->mAnimations[animIndex];
    m_currentAnimName = m_currentAnimation->mName.C_Str();
    m_currentTime = 0.0f;
    m_ticksPerSecond = m_currentAnimation->mTicksPerSecond > 0.0
                       ? static_cast<float>(m_currentAnimation->mTicksPerSecond)
                       : 30.0f;
    m_loop = loop;
    std::cout << "[Animator] Playing: " << m_currentAnimName
              << " (duration=" << m_currentAnimation->mDuration / m_ticksPerSecond << "s)" << std::endl;
}

void Animator::update(float deltaTime) {
    if (!m_currentAnimation || !m_scene || !m_model) return;

    m_currentTime += deltaTime * m_ticksPerSecond;

    float duration = static_cast<float>(m_currentAnimation->mDuration);
    if (m_loop) {
        m_currentTime = fmodf(m_currentTime, duration);
    } else if (m_currentTime > duration) {
        m_currentTime = duration;
    }

    // Partir du RootNode d'Assimp (contient l'échelle FBX, souvent ×0.01)
    // et non du nœud "root" du squelette, sinon les matrices bone
    // manquent la transformation globale du fichier → échelle ×100 → clipping.
    if (m_model->getRootNode()) {
        computeBoneTransform(m_model->getRootNode(), glm::mat4(1.0f), m_currentTime);
    }
}

void Animator::computeBoneTransform(const aiNode* node, const glm::mat4& parentTransform, float animTime) {
    std::string nodeName(node->mName.C_Str());
    auto& boneMap = m_model->getBoneInfoMap();
    bool isBone = (boneMap.find(nodeName) != boneMap.end());

    // Bind-pose par défaut (utilisé pour les nœuds non-bones comme ArmsRig,
    // RootNode, camera, IK handles — leur quaternion FBX est instable).
    glm::mat4 nodeTransform = aiMatrixToGlm(node->mTransformation);

    // N'animer QUE les vrais bones (présents dans boneInfoMap). Les nœuds
    // conteneurs (ArmsRig, RootNode...) restent en bind-pose, ce qui évite
    // les artefacts de quaternion FBX (échelle ×100, matrices non-orthonormales).
    if (isBone && m_currentAnimation) {
        for (unsigned int i = 0; i < m_currentAnimation->mNumChannels; i++) {
            const aiNodeAnim* channel = m_currentAnimation->mChannels[i];
            if (nodeName == channel->mNodeName.C_Str()) {
                nodeTransform = interpolateNodeTransform(channel, animTime);
                break;
            }
        }
    }

    glm::mat4 globalTransform = parentTransform * nodeTransform;

    // Stocker la matrice finale si c'est un bone
    if (isBone) {
        auto it = boneMap.find(nodeName);
        int boneId = it->second.id;
        if (boneId >= 0 && boneId < MAX_BONES) {
            m_finalBoneMatrices[boneId] = globalTransform * it->second.offsetMatrix;
        }
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        computeBoneTransform(node->mChildren[i], globalTransform, animTime);
    }
}

glm::mat4 Animator::interpolateNodeTransform(const aiNodeAnim* channel, float animTime) const {
    glm::vec3 translation = interpolateTranslation(animTime, channel);
    glm::quat rotation = interpolateRotation(animTime, channel);
    // IGNORER l'échelle d'animation : le FBX stocke un ×100 global
    // dans les keyframes, mais les vertices sont déjà à la bonne taille.
    // Appliquer cette échelle via les bones les rend 100× trop grands
    // → frustum clipping. On garde T*R uniquement (les bones ne scalent
    // quasiment jamais en animation squelettique).

    glm::mat4 T = glm::translate(glm::mat4(1.0f), translation);
    glm::mat4 R = glm::mat4_cast(rotation);

    return T * R;
}

glm::vec3 Animator::interpolateTranslation(float animTime, const aiNodeAnim* channel) const {
    if (channel->mNumPositionKeys == 1) {
        return glm::vec3(channel->mPositionKeys[0].mValue.x,
                         channel->mPositionKeys[0].mValue.y,
                         channel->mPositionKeys[0].mValue.z);
    }

    unsigned int idx = findKeyIndex(animTime, channel->mPositionKeys, channel->mNumPositionKeys);
    unsigned int nextIdx = (idx + 1) % channel->mNumPositionKeys;

    float t0 = static_cast<float>(channel->mPositionKeys[idx].mTime);
    float t1 = static_cast<float>(channel->mPositionKeys[nextIdx].mTime);
    float factor = (t1 > t0) ? (animTime - t0) / (t1 - t0) : 0.0f;
    factor = glm::clamp(factor, 0.0f, 1.0f);

    aiVector3D v0 = channel->mPositionKeys[idx].mValue;
    aiVector3D v1 = channel->mPositionKeys[nextIdx].mValue;

    glm::vec3 result = glm::mix(
        glm::vec3(v0.x, v0.y, v0.z),
        glm::vec3(v1.x, v1.y, v1.z),
        factor
    );
    return result;
}

glm::quat Animator::interpolateRotation(float animTime, const aiNodeAnim* channel) const {
    if (channel->mNumRotationKeys == 1) {
        aiQuaternion q = channel->mRotationKeys[0].mValue;
        return glm::quat(q.w, q.x, q.y, q.z);
    }

    unsigned int idx = findKeyIndex(animTime, channel->mRotationKeys, channel->mNumRotationKeys);
    unsigned int nextIdx = (idx + 1) % channel->mNumRotationKeys;

    float t0 = static_cast<float>(channel->mRotationKeys[idx].mTime);
    float t1 = static_cast<float>(channel->mRotationKeys[nextIdx].mTime);
    float factor = (t1 > t0) ? (animTime - t0) / (t1 - t0) : 0.0f;
    factor = glm::clamp(factor, 0.0f, 1.0f);

    aiQuaternion q0 = channel->mRotationKeys[idx].mValue;
    aiQuaternion q1 = channel->mRotationKeys[nextIdx].mValue;

    glm::quat a(q0.w, q0.x, q0.y, q0.z);
    glm::quat b(q1.w, q1.x, q1.y, q1.z);

    // Éviter NaN de slerp quand les quaternions sont (quasi-)identiques
    // dot ≈ ±1.0 → sin(θ) ≈ 0 → division par zéro dans slerp.
    // Si dot < 0, on négocie b pour prendre le chemin court (même rotation).
    float dotProd = glm::dot(a, b);
    if (dotProd < 0.0f) {
        b = -b;
        dotProd = -dotProd;
    }
    if (dotProd > 0.9999f) {
        return a;
    }

    return glm::slerp(a, b, factor);
}

glm::vec3 Animator::interpolateScale(float animTime, const aiNodeAnim* channel) const {
    if (channel->mNumScalingKeys == 1) {
        return glm::vec3(channel->mScalingKeys[0].mValue.x,
                         channel->mScalingKeys[0].mValue.y,
                         channel->mScalingKeys[0].mValue.z);
    }

    unsigned int idx = findKeyIndex(animTime, channel->mScalingKeys, channel->mNumScalingKeys);
    unsigned int nextIdx = (idx + 1) % channel->mNumScalingKeys;

    float t0 = static_cast<float>(channel->mScalingKeys[idx].mTime);
    float t1 = static_cast<float>(channel->mScalingKeys[nextIdx].mTime);
    float factor = (t1 > t0) ? (animTime - t0) / (t1 - t0) : 0.0f;
    factor = glm::clamp(factor, 0.0f, 1.0f);

    aiVector3D s0 = channel->mScalingKeys[idx].mValue;
    aiVector3D s1 = channel->mScalingKeys[nextIdx].mValue;

    return glm::mix(
        glm::vec3(s0.x, s0.y, s0.z),
        glm::vec3(s1.x, s1.y, s1.z),
        factor
    );
}

unsigned int Animator::findKeyIndex(float animTime, const aiVectorKey* keys, unsigned int numKeys) const {
    for (unsigned int i = 0; i < numKeys - 1; i++) {
        if (animTime < static_cast<float>(keys[i + 1].mTime)) {
            return i;
        }
    }
    return numKeys - 1;
}

unsigned int Animator::findKeyIndex(float animTime, const aiQuatKey* keys, unsigned int numKeys) const {
    for (unsigned int i = 0; i < numKeys - 1; i++) {
        if (animTime < static_cast<float>(keys[i + 1].mTime)) {
            return i;
        }
    }
    return numKeys - 1;
}
