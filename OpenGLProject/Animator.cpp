#include "Animator.h"
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdio>
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

    // Assimp peut rapporter 0.0 (ou une valeur aberrante) pour FBX/glTF :
    // fallback 25 t/s. glTF2 (GLB) → 1000 t/s avec temps en ms, FBX → 30.
    const double ticks = m_currentAnimation->mTicksPerSecond;
    m_ticksPerSecond = (ticks > 0.001) ? static_cast<float>(ticks) : 25.0f;

    // Assimp 5.4.3 corrompt les clés de ROTATION à l'import (1 clé valide sur
    // 4, le reste = bruit : temps ~ -6e66, quaternions |q|~1e17). Sans ce
    // nettoyage, l'interpolation échantillonne les clés pourries → poses
    // aléatoires ("mouvements poltergeist"). À faire AVANT de remplir le cache.
    //
    // Limite connue du contournement : les 3/4 des échantillons sont perdus
    // (données irrécupérables) → résolution temporelle ~7,5 im/s (idle quasi
    // statique : aucun impact visuel ; tir/recul : 4 clés sur 500 ms, saccadé
    // mais pose correcte). La mutation in-place de la scène est sûre : le
    // aiScene appartient au Model et Animator en est l'unique consommateur,
    // et la réparation est idempotente (canaux déjà propres inchangés).
    repairRotationKeys(m_currentAnimation);

    // Cache des canaux par nom : évite la recherche linéaire à chaque frame.
    m_channelMap.clear();
    for (unsigned int i = 0; i < m_currentAnimation->mNumChannels; i++) {
        const aiNodeAnim* channel = m_currentAnimation->mChannels[i];
        m_channelMap[channel->mNodeName.C_Str()] = channel;
    }

    m_loop = loop;
    std::cout << "[Animator] Playing: " << m_currentAnimName
              << " (duration=" << m_currentAnimation->mDuration / m_ticksPerSecond << "s)" << std::endl;
}

void Animator::repairRotationKeys(const aiAnimation* anim) {
    if (!anim) return;
    const float duration = static_cast<float>(anim->mDuration);
    unsigned int repairedChannels = 0;
    unsigned int repairedKeys = 0;

    for (unsigned int c = 0; c < anim->mNumChannels; c++) {
        aiNodeAnim* ch = anim->mChannels[c];
        const unsigned int n = ch->mNumRotationKeys;
        if (n == 0) continue;

        // Conserver les clés valides : temps fini dans [0, durée], quaternion
        // unitaire, temps croissants (monotonie). Les clés pourries sont
        // écartées et les bonnes compactées en tête de tableau.
        unsigned int kept = 0;
        float lastTime = -FLT_MAX;
        for (unsigned int i = 0; i < n; i++) {
            aiQuatKey& k = ch->mRotationKeys[i];
            const float t = static_cast<float>(k.mTime);
            const glm::quat q(k.mValue.w, k.mValue.x, k.mValue.y, k.mValue.z);
            const float len = glm::length(q);
            const bool timeOk = std::isfinite(t) && t >= 0.0f && t <= duration + 1.0f && t >= lastTime;
            const bool quatOk = std::isfinite(len) && len > 0.9f && len < 1.1f;
            if (timeOk && quatOk) {
                if (kept != i) ch->mRotationKeys[kept] = k;
                kept++;
                lastTime = t;
            }
        }

        if (kept != n) {
            repairedChannels++;
            repairedKeys += n - kept;
        }
        if (kept == 0) {
            // Tout était du bruit : 0 clé → interpolateRotation retombe sur la
            // bind pose du nœud (defaultValue) plutôt que sur des déchets.
            ch->mNumRotationKeys = 0;
        } else {
            ch->mNumRotationKeys = kept;
        }
    }

    if (repairedChannels > 0) {
        printf("[Animator] repairRotationKeys: %u canaux nettoyes (%u cles pourries)"
               " — bug Assimp 5.4.3 contourne\n", repairedChannels, repairedKeys);
    }
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

    // Animer TOUT nœud possédant un canal d'animation (m_channelMap), pas
    // seulement les bones : si un nœud conteneur (root/ArmsRig/camera/IK) est
    // animé et qu'on le laisse en bind-pose, ses enfants bougent en repère
    // local par rapport à un parent figé → désynchronisation de la hiérarchie.
    // Quaternions dégénérés et échelles ×100 sont neutralisés par
    // interpolateNodeTransform (normalisation + gardes NaN + échelle ignorée).
    auto chanIt = m_channelMap.find(nodeName);
    if (chanIt != m_channelMap.end()) {
        nodeTransform = interpolateNodeTransform(node, chanIt->second, animTime);
    }

    glm::mat4 globalTransform = parentTransform * nodeTransform;

    // Transformation transmise aux enfants. Si ce bone a un offset manuel, il
    // est PROPAGE aux descendants (FK réel) : sans ça, un offset sur
    // upper_arm déplaçait le segment du bras mais PAS l'avant-bras ni la main
    // (les enfants recevaient globalTransform sans l'offset) → les mains
    // restaient collées au corps quelle que soit la pose. C'était la cause du
    // "bras joints" du viewmodel.
    glm::mat4 childTransform = globalTransform;

    // Stocker la matrice finale si c'est un bone
    if (isBone) {
        auto it = boneMap.find(nodeName);
        int boneId = it->second.id;
        if (boneId >= 0 && boneId < MAX_BONES) {
            glm::mat4 finalMat = globalTransform * it->second.offsetMatrix;

            // Offset manuel optionnel : transformation locale supplémentaire
            // appliquée à ce bone précis (ex: décaler un bras). Insérée entre
            // la transformation globale et l'offsetMatrix → espace local bone.
            auto offIt = m_boneOffsets.find(nodeName);
            if (offIt != m_boneOffsets.end()) {
                finalMat = globalTransform * offIt->second * it->second.offsetMatrix;
                // Propager l'offset aux enfants (coude/main suivent le bras).
                childTransform = globalTransform * offIt->second;
            }

            // Garde anti-NaN : keyframes pourries (quaternions degeneres) →
            // matrice non-finie → vertices NaN → rendu invisible. On retombe
            // sur la pose de repos (identite) plutot que d'envoyer du NaN au GPU.
            bool finite = true;
            for (int c = 0; c < 4 && finite; c++) {
                for (int r = 0; r < 4; r++) {
                    if (!std::isfinite(finalMat[c][r])) { finite = false; break; }
                }
            }
            m_finalBoneMatrices[boneId] = finite ? finalMat : glm::mat4(1.0f);
        }
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        computeBoneTransform(node->mChildren[i], childTransform, animTime);
    }
}

glm::mat4 Animator::interpolateNodeTransform(const aiNode* node, const aiNodeAnim* channel, float animTime) const {
    // Bind-pose du nœud (node->mTransformation) : translations/rotations par
    // défaut dans le fichier. Si le canal d'animation n'a PAS de clé pour un
    // composant (position OU rotation), on retombe dessus au lieu de
    // (0,0,0)/identité — sinon les bones (épaules, bras...) perdent leur
    // position de repos et se collent à l'origine du rig (bras "fusionnés"),
    // ou pivotent vers +Y (bras pointant vers le haut).
    const glm::mat4 bindPose = aiMatrixToGlm(node->mTransformation);

    // Translation = colonne 3. Rotation = colonnes normalisées (on retire
    // l'échelle éventuelle du nœud — y compris le ×100 FBX, ignoré partout).
    const glm::vec3 bindTranslation(bindPose[3]);
    const glm::vec3 col0(bindPose[0]), col1(bindPose[1]), col2(bindPose[2]);
    const float sx = glm::length(col0);
    const float sy = glm::length(col1);
    const float sz = glm::length(col2);
    const glm::mat3 rot3(
        (sx > 0.0001f) ? col0 / sx : col0,
        (sy > 0.0001f) ? col1 / sy : col1,
        (sz > 0.0001f) ? col2 / sz : col2);
    const glm::quat bindRotation = glm::quat_cast(rot3);

    glm::vec3 translation = interpolateTranslation(animTime, channel, bindTranslation);
    glm::quat rotation = interpolateRotation(animTime, channel, bindRotation);
    // IGNORER l'échelle d'animation : le FBX stocke un ×100 global
    // dans les keyframes, mais les vertices sont déjà à la bonne taille.
    // Appliquer cette échelle via les bones les rend 100× trop grands
    // → frustum clipping. On garde T*R uniquement (les bones ne scalent
    // quasiment jamais en animation squelettique).

    glm::mat4 T = glm::translate(glm::mat4(1.0f), translation);
    glm::mat4 R = glm::mat4_cast(rotation);

    return T * R;
}

glm::vec3 Animator::interpolateTranslation(float animTime, const aiNodeAnim* channel,
                                           const glm::vec3& defaultValue) const {
    // Bind pose du nœud par défaut (node->mTransformation) : si le canal n'a
    // pas de clé de position, on la conserve au lieu de (0,0,0). C'est elle
    // qui écarte les épaules/bras du centre du rig : sans ce fallback, tous
    // les bones sans clé de translation se collent à l'origine (bras
    // "fusionnés"). La garde anti-NaN en fin de fonction valide la valeur.
    glm::vec3 result = defaultValue;

    if (channel->mNumPositionKeys == 1) {
        result = glm::vec3(channel->mPositionKeys[0].mValue.x,
                           channel->mPositionKeys[0].mValue.y,
                           channel->mPositionKeys[0].mValue.z);
    } else if (channel->mNumPositionKeys > 1) {
        unsigned int idx = findKeyIndex(animTime, channel->mPositionKeys, channel->mNumPositionKeys);
        unsigned int nextIdx = idx + 1;

        // CORRECTION : en fin de cycle, ne PAS boucler sur la première clé
        // ((idx+1) % numKeys → 0) : on reste sur la dernière clé au lieu
        // d'interpoler la dernière pose avec la première (saut/spasme).
        if (nextIdx >= channel->mNumPositionKeys) {
            const aiVector3D& v = channel->mPositionKeys[idx].mValue;
            const glm::vec3 last(v.x, v.y, v.z);
            if (!std::isfinite(last.x) || !std::isfinite(last.y) || !std::isfinite(last.z)) {
                // Garde anti-NaN (comme en fin de fonction) : bind pose si finie.
                if (std::isfinite(defaultValue.x) && std::isfinite(defaultValue.y) && std::isfinite(defaultValue.z)) {
                    return defaultValue;
                }
                return glm::vec3(0.0f);
            }
            return last;
        }

        float t0 = static_cast<float>(channel->mPositionKeys[idx].mTime);
        float t1 = static_cast<float>(channel->mPositionKeys[nextIdx].mTime);
        float factor = (t1 > t0) ? (animTime - t0) / (t1 - t0) : 0.0f;
        factor = glm::clamp(factor, 0.0f, 1.0f);

        aiVector3D v0 = channel->mPositionKeys[idx].mValue;
        aiVector3D v1 = channel->mPositionKeys[nextIdx].mValue;

        result = glm::mix(
            glm::vec3(v0.x, v0.y, v0.z),
            glm::vec3(v1.x, v1.y, v1.z),
            factor
        );
    }

    // Garde anti-NaN : translation non-finie → bind pose du nœud si elle est
    // finie, sinon (0,0,0) (au moins fini — on évite d'envoyer du NaN au GPU).
    if (!std::isfinite(result.x) || !std::isfinite(result.y) || !std::isfinite(result.z)) {
        if (std::isfinite(defaultValue.x) && std::isfinite(defaultValue.y) && std::isfinite(defaultValue.z)) {
            return defaultValue;
        }
        return glm::vec3(0.0f);
    }
    return result;
}

glm::quat Animator::interpolateRotation(float animTime, const aiNodeAnim* channel,
                                        const glm::quat& defaultValue) const {
    // Rotation de bind pose par défaut (node->mTransformation) : si le canal
    // n'a pas de clé de rotation, on la conserve. Sans ce fallback, le bone
    // retomberait sur l'identité → aligné sur l'axe Y du parent → bras
    // pointant vers le haut au lieu de leur pose exportée. La garde finale
    // valide la valeur (bind dégénérée → identité).
    glm::quat result = defaultValue;

    if (channel->mNumRotationKeys == 1) {
        aiQuaternion q = channel->mRotationKeys[0].mValue;
        result = glm::quat(q.w, q.x, q.y, q.z);
    } else if (channel->mNumRotationKeys > 1) {
        unsigned int idx = findKeyIndex(animTime, channel->mRotationKeys, channel->mNumRotationKeys);
        unsigned int nextIdx = idx + 1;

        // CORRECTION : en fin de cycle, ne PAS boucler sur la première clé
        // (voir interpolateTranslation) — on reste sur la dernière clé.
        if (nextIdx >= channel->mNumRotationKeys) {
            const aiQuaternion& q = channel->mRotationKeys[idx].mValue;
            const glm::quat res(q.w, q.x, q.y, q.z);
            const float rl = glm::length(res);
            return (std::isfinite(rl) && rl > 0.0001f) ? glm::normalize(res) : defaultValue;
        }

        float t0 = static_cast<float>(channel->mRotationKeys[idx].mTime);
        float t1 = static_cast<float>(channel->mRotationKeys[nextIdx].mTime);
        float factor = (t1 > t0) ? (animTime - t0) / (t1 - t0) : 0.0f;
        factor = glm::clamp(factor, 0.0f, 1.0f);

        aiQuaternion q0 = channel->mRotationKeys[idx].mValue;
        aiQuaternion q1 = channel->mRotationKeys[nextIdx].mValue;

        glm::quat a(q0.w, q0.x, q0.y, q0.z);
        glm::quat b(q1.w, q1.x, q1.y, q1.z);

        // Normaliser AVANT slerp. Blender exporte parfois des quaternions
        // non-unitaires voire (0,0,0,0) pour des canaux non-animes ; dans
        // mat4_cast ils produisent des matrices d'echelle ~1e20 (mesure :
        // boneMatrices a 7.4e19 !) → vertices hors frustum → bras invisibles.
        const float lenA = glm::length(a);
        const float lenB = glm::length(b);
        a = (std::isfinite(lenA) && lenA > 0.0001f)
                ? (a / lenA)
                : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        b = (std::isfinite(lenB) && lenB > 0.0001f)
                ? (b / lenB)
                : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

        // Éviter NaN de slerp quand les quaternions sont (quasi-)identiques
        // dot ≈ ±1.0 → sin(θ) ≈ 0 → division par zéro dans slerp.
        // Si dot < 0, on négocie b pour prendre le chemin court (même rotation).
        float dotProd = glm::dot(a, b);
        if (dotProd < 0.0f) {
            b = -b;
            dotProd = -dotProd;
        }
        if (dotProd > 0.9999f) {
            result = a;
        } else {
            result = glm::slerp(a, b, factor);
        }
    }

    // Garde finale : quaternion non-fini ou (quasi-)nul → on retombe sur la
    // rotation de bind pose si elle est valide, sinon l'identité.
    const float len = glm::length(result);
    if (!std::isfinite(len) || len < 0.0001f) {
        const float dLen = glm::length(defaultValue);
        if (std::isfinite(dLen) && dLen > 0.0001f) {
            return glm::normalize(defaultValue);
        }
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    return glm::normalize(result);
}

glm::vec3 Animator::interpolateScale(float animTime, const aiNodeAnim* channel) const {
    // Non utilisé actuellement (l'échelle d'animation FBX ×100 est ignorée
    // volontairement dans interpolateNodeTransform). Garde anti-débordement :
    // 0 clé → échelle neutre (sinon findKeyIndex déborde en unsigned).
    if (channel->mNumScalingKeys == 0) {
        return glm::vec3(1.0f);
    }

    if (channel->mNumScalingKeys == 1) {
        return glm::vec3(channel->mScalingKeys[0].mValue.x,
                         channel->mScalingKeys[0].mValue.y,
                         channel->mScalingKeys[0].mValue.z);
    }

    unsigned int idx = findKeyIndex(animTime, channel->mScalingKeys, channel->mNumScalingKeys);
    unsigned int nextIdx = idx + 1;

    // CORRECTION : en fin de cycle, ne PAS boucler sur la première clé.
    if (nextIdx >= channel->mNumScalingKeys) {
        const aiVector3D& s = channel->mScalingKeys[idx].mValue;
        const glm::vec3 last(s.x, s.y, s.z);
        return (std::isfinite(last.x) && std::isfinite(last.y) && std::isfinite(last.z))
               ? last : glm::vec3(1.0f);
    }

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
    // Garde anti-débordement : numKeys==0 → numKeys-1 déborde en unsigned
    // (boucle infinie + lecture hors bornes). Les appelants gèrent le cas
    // 0 clé via leur defaultValue.
    if (numKeys == 0) {
        return 0;
    }

    for (unsigned int i = 0; i < numKeys - 1; i++) {
        if (animTime < static_cast<float>(keys[i + 1].mTime)) {
            return i;
        }
    }
    return numKeys - 1;
}

unsigned int Animator::findKeyIndex(float animTime, const aiQuatKey* keys, unsigned int numKeys) const {
    // Garde anti-débordement : numKeys==0 → numKeys-1 déborde en unsigned
    // (boucle infinie + lecture hors bornes). Les appelants gèrent le cas
    // 0 clé via leur defaultValue.
    if (numKeys == 0) {
        return 0;
    }

    for (unsigned int i = 0; i < numKeys - 1; i++) {
        if (animTime < static_cast<float>(keys[i + 1].mTime)) {
            return i;
        }
    }
    return numKeys - 1;
}
