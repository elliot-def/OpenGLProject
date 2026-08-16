#pragma once
#include <assimp/scene.h>
#include <glm/glm.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "SkinningData.h"

// Stub minimal de Model pour compiler Animator.cpp hors du jeu.
class Model {
public:
    void setScene(const aiScene* scene) { m_scene = scene; }
    const aiScene* getScene() const { return m_scene; }
    const aiNode* getRootNode() const { return m_scene ? m_scene->mRootNode : nullptr; }
    const std::unordered_map<std::string, BoneInfo>& getBoneInfoMap() const { return m_boneInfoMap; }
    const std::unordered_set<const aiNode*>& getJointNodes() const { return m_jointNodes; }
    const aiAnimation* getAnimation(size_t index) const {
        if (!m_scene) return nullptr;
        if (index < m_scene->mNumAnimations)
            return m_scene->mAnimations[index];
        index -= m_scene->mNumAnimations;
        if (index < m_externalAnimations.size())
            return m_externalAnimations[index];
        return nullptr;
    }
    void addExternalAnimation(const aiAnimation* anim) { m_externalAnimations.push_back(anim); }
    size_t numExternal() const { return m_externalAnimations.size(); }

    // Remplit boneInfoMap comme le ferait processMesh (pour le diagnostic
    // "bones figes" et l'isBone). On enregistre les noeuds nommes
    // Left/Right* et les helpers RrTt associes.
    void registerBone(const std::string& name, int id) {
        m_boneInfoMap[name].id = id;
        m_jointNodes.insert(nullptr); // non utilise par notre parcours
    }

private:
    const aiScene* m_scene = nullptr;
    std::unordered_map<std::string, BoneInfo> m_boneInfoMap;
    std::unordered_set<const aiNode*> m_jointNodes;
    std::vector<const aiAnimation*> m_externalAnimations;
};
