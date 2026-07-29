#include "Animator.h"

// GLM_ENABLE_EXPERIMENTAL doit etre defini AVANT tout include d'un header
// glm/gtx/* (experimental). Sinon <glm/gtx/quaternion.hpp> tire
// <glm/gtx/component_wise.hpp> qui #error en C1189 sous MSVC. Meme approche
// dans CollisionManager.cpp.
#define GLM_ENABLE_EXPERIMENTAL

#include <assimp/scene.h>
#include <glm/gtx/quaternion.hpp>
#include <cmath>
#include <algorithm>

// SLERP helper : interpolation spherique pour quaternions.
// glm::slerp existe mais on implemente une version localement pour eviter
// les soucis de precision sur de tres petits intervals de temps.
static glm::quat slerpQuat(glm::quat a, glm::quat b, double t) {
    // Normalisation securite.
    a = glm::normalize(a);
    b = glm::normalize(b);
    double dot = glm::dot(a, b);

    // Si dot < 0, shortest-path : inverser b.
    if (dot < 0.0) { b = -b; dot = -dot; }

    // Si tres proche, fallback lerp + normalize.
    if (dot > 0.9995) {
        glm::quat out = glm::mix(a, b, static_cast<float>(t));
        return glm::normalize(out);
    }

    double theta_0 = std::acos(dot);
    double theta   = theta_0 * t;
    double sin_theta_0 = std::sin(theta_0);

    double s0 = std::cos(theta) - dot * std::sin(theta) / sin_theta_0;
    double s1 = std::sin(theta) / sin_theta_0;

    return static_cast<float>(s0) * a + static_cast<float>(s1) * b;
}

Animator::Animator() = default;

Animator::~Animator() {
    // Liberation du snapshot de hierarchie (alloue dans setup()).
    //
    // BUG ORIGINAL : la boucle faisait push(&node->children[i]) puis
    // delete[] node->children. Les adresses &node->children[i] pointent
    // A L'INTERIEUR du bloc qui vient d'etre libere -> au pop suivant, le
    // pointeur lu est dangling -> ACCESS VIOLATION (typiquement
    // 0xFFFFFFFFFFFFFFFF sur MSVC apres free).
    //
    // FIX : on releve d'abord tous les tableaux .children[] dans un vecteur
    // d'adresses, puis on delete[] tout en une seule passe a la fin. Aucun
    // delete[] ne survient tant que la pile contient encore des pointeurs
    // qui pourraient pointer dans un bloc libere.
    //
    // L'ordre de liberation est libre : on delete[] uniquement les TABLEAUX
    // (pas les AssimpNodeData eux-memes), et chaque AssimpNodeData vit dans
    // le .children[] de son PARENT (sauf les enfants directs de la racine
    // qui vivent dans m_rootNodeCopy.children, libere en toute fin).
    if (m_hasRootNode && m_rootNodeCopy.children && m_rootNodeCopy.childrenCount > 0) {
        std::vector<AssimpNodeData*> stack;
        std::vector<AssimpNodeData**> arraysToFree; // adresses des champs .children
        stack.reserve(64);
        arraysToFree.reserve(64);

        // Empile les enfants directs de la racine.
        for (int i = m_rootNodeCopy.childrenCount - 1; i >= 0; --i) {
            stack.push_back(&m_rootNodeCopy.children[i]);
        }

        // DFS pre-order (nul besoin de post-order ici : on ne delete[]
        // rien pendant la traversee, juste on releve les adresses).
        while (!stack.empty()) {
            AssimpNodeData* node = stack.back();
            stack.pop_back();
            if (node->childrenCount > 0 && node->children) {
                arraysToFree.push_back(&node->children);
                for (int i = node->childrenCount - 1; i >= 0; --i) {
                    stack.push_back(&node->children[i]);
                }
            }
        }

        // Passe de liberation finale. ATTENTION : on itere dans le SENS INVERSE.
        //
        // Pourquoi : le DFS est pre-order, donc on a empile les .children[] AVANT
        // de descendre dans le sous-arbre. arraysToFree contient donc les
        // pointeurs dans l'ordre [&parent.children, &enfant1.children, ...].
        //
        // Si on iterait forward, le premier delete[] libererait
        // `parent.children[]` -- le tableau qui contient justement `enfant1`,
        // `enfant2`, ... ; donc le 2eme `*arr` (=&enfant1.children)
        // lirait de la memoire liberee -> garbage 0xFFFFFFFFFFFFFFFF ->
        // crash sur delete[] *arr.
        //
        // En sens inverse, on libere d'abord les tableaux les plus profonds
        // (enfants avant parents), donc chaque `*arr` reste valide tant qu'il
        // n'a pas ete libere lui-meme.
        while (!arraysToFree.empty()) {
            AssimpNodeData** arr = arraysToFree.back();
            arraysToFree.pop_back();
            delete[] *arr;
            *arr = nullptr;
        }
        delete[] m_rootNodeCopy.children;
        m_rootNodeCopy.children = nullptr;
        m_rootNodeCopy.childrenCount = 0;
    }
    m_hasRootNode = false;
}

void Animator::setup(const aiNode* rootNode,
                     const std::unordered_map<std::string, BoneInfo>& boneMap,
                     const std::vector<AnimationClip>& clips) {
    m_rootNodePtr = rootNode;
    m_boneMap     = boneMap;
    m_clips       = clips;

    // Indexation des clips par nom.
    m_clipNameToIndex.reserve(m_clips.size());
    for (int i = 0; i < static_cast<int>(m_clips.size()); ++i) {
        m_clipNameToIndex[m_clips[i].name] = i;
    }

    // Snapshot de la hierarchie assimp (pour eviter de dependre du lifecycle
    // du aiScene pendant l'update).
    m_hasRootNode = false;
    if (m_rootNodePtr) {
        m_rootNodeCopy = AssimpNodeData{};
        readNodeHierarchy(m_rootNodePtr, m_rootNodeCopy);
        m_hasRootNode = true;
    }

    // Resize du buffer de sortie (un slot par bone).
    int maxId = -1;
    for (const auto& [name, info] : m_boneMap) {
        if (info.id > maxId) maxId = info.id;
    }
    m_finalBoneMatrices.assign(maxId + 1, glm::mat4(1.0f));
}

glm::vec3 Animator::samplePosition(const AnimationChannel& ch, double t) {
    if (ch.positionKeys.empty()) return glm::vec3(0.0f);
    if (ch.positionKeys.size() == 1) return ch.positionKeys[0].value;
    // Bornage.
    if (t <= ch.positionKeys.front().time) return ch.positionKeys.front().value;
    if (t >= ch.positionKeys.back().time)  return ch.positionKeys.back().value;

    // Recherche dichotomique de l'intervalle [t0, t1].
    auto it = std::lower_bound(ch.positionKeys.begin(), ch.positionKeys.end(), t,
        [](const PositionKey& k, double v) { return k.time < v; });
    if (it == ch.positionKeys.begin()) return ch.positionKeys.front().value;

    const auto& a = *(it - 1);
    const auto& b = *it;
    double factor = (t - a.time) / std::max(b.time - a.time, 1e-8);
    return glm::mix(a.value, b.value, static_cast<float>(factor));
}

glm::quat Animator::sampleRotation(const AnimationChannel& ch, double t) {
    if (ch.rotationKeys.empty()) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    if (ch.rotationKeys.size() == 1) return ch.rotationKeys[0].value;
    if (t <= ch.rotationKeys.front().time) return ch.rotationKeys.front().value;
    if (t >= ch.rotationKeys.back().time)  return ch.rotationKeys.back().value;

    auto it = std::lower_bound(ch.rotationKeys.begin(), ch.rotationKeys.end(), t,
        [](const RotationKey& k, double v) { return k.time < v; });
    if (it == ch.rotationKeys.begin()) return ch.rotationKeys.front().value;

    const auto& a = *(it - 1);
    const auto& b = *it;
    double factor = (t - a.time) / std::max(b.time - a.time, 1e-8);
    return slerpQuat(a.value, b.value, factor);
}

glm::vec3 Animator::sampleScale(const AnimationChannel& ch, double t) {
    if (ch.scaleKeys.empty()) return glm::vec3(1.0f);
    if (ch.scaleKeys.size() == 1) return ch.scaleKeys[0].value;
    if (t <= ch.scaleKeys.front().time) return ch.scaleKeys.front().value;
    if (t >= ch.scaleKeys.back().time)  return ch.scaleKeys.back().value;

    auto it = std::lower_bound(ch.scaleKeys.begin(), ch.scaleKeys.end(), t,
        [](const ScaleKey& k, double v) { return k.time < v; });
    if (it == ch.scaleKeys.begin()) return ch.scaleKeys.front().value;

    const auto& a = *(it - 1);
    const auto& b = *it;
    double factor = (t - a.time) / std::max(b.time - a.time, 1e-8);
    return glm::mix(a.value, b.value, static_cast<float>(factor));
}

glm::mat4 Animator::channelLocalTransform(const AnimationChannel& ch, double t) {
    glm::vec3 T = samplePosition(ch, t);
    glm::quat R = sampleRotation(ch, t);
    glm::vec3 S = sampleScale(ch, t);

    // Convention GLM : TRS = T * R * S  (scale > rotate > translate dans le local).
    glm::mat4 m = glm::translate(glm::mat4(1.0f), T)
                * glm::mat4_cast(R)
                * glm::scale(glm::mat4(1.0f), S);
    return m;
}

void Animator::readNodeHierarchy(const aiNode* src, AssimpNodeData& dst) {
    if (!src) return;
    dst.name = src->mName.C_Str();
    dst.transformation = aiMatrixToGlm(src->mTransformation);
    dst.childrenCount = src->mNumChildren;
    if (dst.childrenCount > 0) {
        dst.children = new AssimpNodeData[dst.childrenCount];
        for (unsigned i = 0; i < src->mNumChildren; ++i) {
            readNodeHierarchy(src->mChildren[i], dst.children[i]);
        }
    }
}

void Animator::playClip(const std::string& clipName) {
    auto it = m_clipNameToIndex.find(clipName);
    if (it != m_clipNameToIndex.end()) {
        m_currentClipIndex = it->second;
        m_currentTime = 0.0;
    }
}

void Animator::update(float deltaTime) {
    if (!m_hasRootNode || m_currentClipIndex < 0) return;

        const AnimationClip& clip = m_clips[m_currentClipIndex];
        m_currentTime += static_cast<double>(deltaTime) * clip.ticksPerSecond;
        if (clip.duration > 0.0 && m_currentTime > clip.duration) {
            m_currentTime = std::fmod(m_currentTime, clip.duration);
        }

    std::unordered_map<std::string, const AnimationChannel*> channelByName;
        for (const auto& ch : clip.channels) channelByName[ch.nodeName] = &ch;

            std::fill(m_finalBoneMatrices.begin(), m_finalBoneMatrices.end(), glm::mat4(1.0f));

            // Matrice inverse de la racine pour ramener les os dans l'espace local du Mesh
            glm::mat4 globalInverse = glm::inverse(m_rootNodeCopy.transformation);

    struct StackEntry {
        const AssimpNodeData* node;
            glm::mat4 parentGlobal;
    };
    std::vector<StackEntry> stack;
        stack.push_back({ &m_rootNodeCopy, glm::mat4(1.0f) });

        while (!stack.empty()) {
            StackEntry cur = stack.back();
                stack.pop_back();

                glm::mat4 local = cur.node->transformation;
                auto itCh = channelByName.find(cur.node->name);
                if (itCh != channelByName.end()) {
                    local = channelLocalTransform(*(itCh->second), m_currentTime);
                }

            glm::mat4 global = cur.parentGlobal * local;

                auto itBone = m_boneMap.find(cur.node->name);
                if (itBone != m_boneMap.end()) {
                    if (itBone->second.id >= 0 && itBone->second.id < static_cast<int>(m_finalBoneMatrices.size())) {
                        // Multiplier par globalInverse évite le décalage/étirement bizarre du Mesh
                        m_finalBoneMatrices[itBone->second.id] = globalInverse * global * itBone->second.offsetMatrix;
                    }
                }

            if (cur.node->childrenCount > 0 && cur.node->children) {
                for (int i = cur.node->childrenCount - 1; i >= 0; --i) {
                    stack.push_back({ &cur.node->children[i], global });
                }
            }
        }
}