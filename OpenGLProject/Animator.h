#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// Animator : pilote la lecture d'une (ou plusieurs) AnimationClip sur un
// squelette issu de Model.
//
// A chaque update(dt) :
//   1. m_currentTime avance (TicksPerSecond * dt).
//   2. Pour chaque clip actif dans m_activeClips (poids sommés à 1) :
//      - Lecture de chaque channel interpolatee a m_currentTime.
//      - Composition d'une transformation locale par bone.
//   3. Recursivement, traverse le aiNode* de la scene (le rootNode, pas
//      seulement les bones) pour accumuler les transformations globales.
//   4. Pour chaque bone (id dans la boneMap), multiplie globalTransform *
//      offsetMatrix et stocke le resultat dans m_finalBoneMatrices.
//
// Note : la traversee se fait sur TOUS les noeuds (incluant les non-bones)
// parce que les transformations hierarchiques doivent être propagees aux
// enfants meme si un noeud parent n'est pas lui-meme un bone. Seuls les
// resultats pour les os sont envoyes au GPU.
// ─────────────────────────────────────────────────────────────────────────────

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <unordered_map>

#include "SkinningData.h"
#include "AnimationClip.h"

struct aiNode; // fwd decl

class Animator {
public:
    Animator();
    ~Animator(); // Libere recursivement m_rootNodeCopy.children

    // Setups : appelle une seule fois apres le chargement du Model.
    // - rootNode : scene->mRootNode (lifecycle lie au Model).
    // - boneMap   : map nom->BoneInfo, construite par Model::processMesh.
    // - clips     : vector d'AnimationClip extruits depuis aiScene->mAnimations.
    void setup(const aiNode* rootNode,
               const std::unordered_map<std::string, BoneInfo>& boneMap,
               const std::vector<AnimationClip>& clips);

    // Advance anim state + recalcule m_finalBoneMatrices.
    void update(float deltaTime);

    // Resultat a envoyer au shader (uBoneMatrices).
    const std::vector<glm::mat4>& getFinalBoneMatrices() const { return m_finalBoneMatrices; }

    // Combien de bones (taille du tableau envoye au GPU).
    int getBoneCount() const { return static_cast<int>(m_finalBoneMatrices.size()); }

    // Selection d'un clip "principal" (mode lecture simple, sans blend).
    // Note : pour crossfade entre Idle/Walk, plusieurs clips peuvent etre actifs.
    void playClip(const std::string& clipName);

    // Definit le temps absolu (utile pour debug / tests deposes).
    void setCurrentTime(double t) { m_currentTime = t; }
    double getCurrentTime() const  { return m_currentTime; }

private:
    // Indexation de la hierarchie assimp pour la recursion.
    struct AssimpNodeData {
        glm::mat4       transformation{ 1.0f };
        std::string     name;
        int             childrenCount = 0;
        AssimpNodeData* children = nullptr;
    };

    // Lecture d'une cle de transformation au temps donne, pour un channel donne.
    static glm::vec3 samplePosition(const AnimationChannel& ch, double t);
    static glm::quat sampleRotation(const AnimationChannel& ch, double t);
    static glm::vec3 sampleScale   (const AnimationChannel& ch, double t);

    // Construit la transformation locale (T*R*S) pour un channel au temps t.
    static glm::mat4 channelLocalTransform(const AnimationChannel& ch, double t);

    // Recursion hierarchique collecte les noeuds depuis l'aiNode root.
    static void readNodeHierarchy(const aiNode* src, AssimpNodeData& dst);

    // Mise a jour recursive : pour chaque noeud, multiplie parentTransform *
    // node.transformation (issue de l'anim blend) et propage aux enfants. Si
    // le nom du noeud est un bone, stocke la matrice calculee dans
    // m_finalBoneMatrices[index].
    void updateNodeHierarchy(const AssimpNodeData* node, const glm::mat4& parentTransform);

    // Pointeur non-proprietaire sur le rootNode de la scene assimp (le Model
    // proprietaire reste en vie tant que l'Animator l'utilise). Si le Model
    // est detruit avant l'Animator, ce pointeur devient dangling -> guard.
    const aiNode* m_rootNodePtr = nullptr;
    std::unordered_map<std::string, BoneInfo> m_boneMap; // copie (lecture seule)
    std::vector<AnimationClip>                m_clips;
    std::unordered_map<std::string, int>      m_clipNameToIndex;

    // Snapshots de hierarchie pour la recursion (own).
    AssimpNodeData m_rootNodeCopy;
    bool           m_hasRootNode = false;

    // Nom du clip en cours.
    int            m_currentClipIndex = -1;
    double         m_currentTime = 0.0;

    // Sortie : matrice finale par bone (offset * global). Indexe par boneId.
    std::vector<glm::mat4> m_finalBoneMatrices;
};
