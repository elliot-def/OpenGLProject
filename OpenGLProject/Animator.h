#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <assimp/scene.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "SkinningData.h"
#include "Model.h"

class Animator {
public:
    Animator() = default;

    // Initialise l'animator avec le modèle (pour accéder au scene et boneInfoMap)
    void setup(Model* model);

    // Joue une animation par son index dans scene->mAnimations[]
    // (delegue a la version par pointeur via Model::getAnimation).
    // crossfade=true : fond la pose de l'animation en cours vers la nouvelle
    // sur kCrossfadeDuration (evite le "pop" entre idle et marche). Passer
    // false pour une coupure franche (ex: le recul du tir).
    void playAnimation(unsigned int animIndex, bool loop = true, bool crossfade = true);

    // Joue une animation par son pointeur aiAnimation (supporte les animations
    // externes chargees via Model::loadExternalAnimations).
    void playAnimation(const aiAnimation* anim, bool loop = true, bool crossfade = true);

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

    // Retourne l'animation en cours (pointeur, pour comparaison sans
    // relancer playAnimation quand l'animation n'a pas changé)
    const aiAnimation* getCurrentAnimation() const { return m_currentAnimation; }

    // Applique un offset local supplémentaire à un bone par son nom (ex:
    // décaler un bras). L'offset est composé avec la transformation du bone
    // à chaque frame, entre la transformation globale et l'offsetMatrix.
    // Passer glm::mat4(1.0f) retire l'offset du bone.
    void addBoneOffset(const std::string& boneName, const glm::mat4& offset) {
        m_boneOffsets[boneName] = offset;
    }

    // Supprime tous les offsets manuels
    void clearBoneOffsets() { m_boneOffsets.clear(); }

    // Retourne la transformation globale (model-space) d'un nœud par son nom.
    // Mis à jour chaque frame dans computeBoneTransform().
    glm::mat4 getGlobalNodeTransform(const std::string& nodeName) const {
        auto it = m_globalNodeTransforms.find(nodeName);
        return it != m_globalNodeTransforms.end() ? it->second : glm::mat4(1.0f);
    }

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

    // Cache des transforms globales (model-space) par nom de nœud.
    // Rempli dans computeBoneTransform() pour O(1) lookup externe.
    std::unordered_map<std::string, glm::mat4> m_globalNodeTransforms;

    // Cache des canaux d'animation par nom de nœud (reconstruit à chaque
    // playAnimation) : évite la recherche linéaire dans mNumChannels à chaque
    // frame, et permet d'animer TOUS les nœuds canalisés (pas seulement les
    // bones) — un conteneur figé désynchronise sa hiérarchie d'enfants.
    std::unordered_map<std::string, const aiNodeAnim*> m_channelMap;

    // FBX importé par Assimp avec la décomposition RrTt : chaque bone est
    // éclaté en nœuds "bone_$AssimpFbx$_Translation" / "bone_$AssimpFbx$_PreRotation"
    // / "bone". Dans ce cas la position vient du wrapper _Translation (bind) et
    // le canal d'animation ne doit fournir QUE la rotation — sinon la
    // translation du canal s'ajoute à celle du wrapper → segments étirés. Les
    // canaux de rotation portent alors le suffixe "_$AssimpFbx$_Rotation".
    bool m_usesFbxRrTtHelpers = false;

    // Les animations sont partagées entre plusieurs transitions fire -> idle.
    // Éviter de retrier/reparcourir leurs clés à chaque transition : ce travail
    // est nécessaire une seule fois par aiAnimation après son import.
    std::unordered_set<const aiAnimation*> m_repairedAnimations;
    std::unordered_set<const aiAnimation*> m_debugPrintedAnimations;

    // ── Crossfade (fondu entre deux animations) ──────────────────────────────
    // Principe : chaque frame, computeBoneTransform() stocke la transformée
    // LOCALE (T·R, relative au parent) de chaque nœud dans
    // m_currentLocalTransforms. Au moment du switch, on la copie dans
    // m_prevLocalTransforms (instantané figé de la dernière pose source).
    // Pendant le fondu, computeBoneTransform() blend chaque transformée locale
    // (lerp translation + slerp rotation) vers l'animation courante, PUIS
    // recompose la hiérarchie normalement. Ainsi la translation et la rotation
    // restent synchrones (pas de colonne 3 contaminée par l'offsetMatrix comme
    // avec l'ancien blend sur matrices finales → plus d'overshoot des doigts).
    static constexpr float kCrossfadeDuration = 0.25f; // durée du fondu (idle <-> marche)
    // Transformées LOCALES de TOUS les nœuds (bones + conteneurs) : mises à jour
    // chaque frame dans computeBoneTransform(). Clé = POINTEUR du nœud (et non
    // son nom) : un fichier peut contenir plusieurs squelettes aux noms de bones
    // IDENTIQUES (ex: human_1.glb → MaleArm + FemaleArm) ; une clé par nom
    // laisserait les nœuds du second rig écraser l'instantané du premier.
    std::unordered_map<const aiNode*, glm::mat4> m_currentLocalTransforms;
    std::unordered_map<const aiNode*, glm::mat4> m_prevLocalTransforms; // instantané
    float m_fade = 1.0f;                               // facteur de blend courant (0→1)
    float m_fadeTimer = 0.0f;                          // progression du fondu (s)
    bool m_crossfading = false;                        // fondu en cours ?

    // Filtre les clés dont le temps ou le quaternion sont manifestement
    // invalides, garde les rotations non normalisées valides en les normalisant,
    // puis compacte le tableau. À appeler dans playAnimation, avant le cache.
    void repairRotationKeys(const aiAnimation* anim);

    // Trouve la transformation d'un nœud à un instant donné dans l'animation.
    // node sert de fallback : si le canal n'a pas de clé pour un composant
    // (position/rotation), on utilise la transformation de repos (bind pose)
    // du nœud au lieu de (0,0,0)/identité — sinon les bones se collent au
    // centre du rig ou pivotent vers +Y (bras collés / pointant vers le haut).
    glm::mat4 interpolateNodeTransform(const aiNode* node, const aiNodeAnim* channel, float animTime) const;

    // Calcule récursivement les matrices globales des bones (animation
    // courante, m_channelMap) dans m_finalBoneMatrices
    void computeBoneTransform(const aiNode* node, const glm::mat4& parentTransform, float animTime);

    // Blende deux matrices de transformée LOCALE (T·R, relative au parent) :
    // translation lerpée + rotation slerpée (chemin court avec garde anti-NaN).
    // Contrairement à l'ancien mixBoneMatrices qui travaillait sur les matrices
    // finales (globalTransform * offsetMatrix), ici on travaille sur du T·R pur
    // → lerp et slerp sont synchrones, pas d'overshoot.
    static glm::mat4 blendLocalTransforms(const glm::mat4& from, const glm::mat4& to, float t);

    // Helper : interpolation de translation (defaultValue = bind pose du nœud,
    // utilisée si le canal n'a pas de clé de position)
    glm::vec3 interpolateTranslation(float animTime, const aiNodeAnim* channel,
                                     const glm::vec3& defaultValue) const;

    // Helper : interpolation de rotation (defaultValue = bind pose du nœud,
    // utilisée si le canal n'a pas de clé de rotation)
    glm::quat interpolateRotation(float animTime, const aiNodeAnim* channel,
                                  const glm::quat& defaultValue) const;

    // Trouve l'index de la keyframe pour un temps donné
    unsigned int findKeyIndex(float animTime, const aiVectorKey* keys, unsigned int numKeys) const;
    unsigned int findKeyIndex(float animTime, const aiQuatKey* keys, unsigned int numKeys) const;
};
