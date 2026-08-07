#include "Animator.h"
#include "Log.h"
#include <algorithm>
#include <cmath>
#include <cfloat>

namespace {
// Mixamo prefixe souvent les noms de bones par "mixamorig:". Mais certains
// exports (personnages "Ch22", re-exports Blender...) utilisent
// "mixamorig1:", "mixamorig2:" etc. Les fichiers d'animation et le modele
// peuvent avoir des prefixes differents : on retire "mixamorig" + chiffres
// + ":" pour que les channels matchent toujours.
std::string stripMixamoPrefix(const std::string& name) {
    const std::string prefix = "mixamorig";
    if (name.size() > prefix.size() &&
        name.compare(0, prefix.size(), prefix) == 0) {
        size_t i = prefix.size();
        while (i < name.size() && name[i] >= '0' && name[i] <= '9') i++;
        if (i < name.size() && name[i] == ':')
            return name.substr(i + 1);
    }
    return name;
}

// Normalise un nom de bone/canal : retire le prefixe Mixamo ("mixamorig:",
// "mixamorig2:", ...) ET le suffixe Assimp FBX "_$AssimpFbx$_Rotation" des
// canaux de rotation des bones avec PreRotation (decomposition RrTt).
// Ex: "mixamorig2:LeftArm_$AssimpFbx$_Rotation" -> "LeftArm".
std::string normalizeNodeName(const std::string& name) {
    std::string n = stripMixamoPrefix(name);
    const std::string rotSuffix = "_$AssimpFbx$_Rotation";
    if (n.size() > rotSuffix.size() &&
        n.compare(n.size() - rotSuffix.size(), rotSuffix.size(), rotSuffix) == 0) {
        n = n.substr(0, n.size() - rotSuffix.size());
    }
    return n;
}
} // namespace

void Animator::setup(Model* model) {
    m_model = model;
    m_scene = model->getScene();
    m_channelMap.clear();
    m_repairedAnimations.clear();
    m_debugPrintedAnimations.clear();
    m_finalBoneMatrices.resize(MAX_BONES, glm::mat4(1.0f));

    // Détecte la decomposition RrTt d'Assimp : un nœud "*_$AssimpFbx$_Translation"
    // dans l'arbre indique un FBX dont les bones ont été éclatés en
    // Translation/PreRotation/bone. Dans ce cas, les canaux d'animation ne
    // fournissent que la rotation (voir interpolateNodeTransform).
    m_usesFbxRrTtHelpers = false;
    if (m_scene && m_scene->mRootNode) {
        std::vector<const aiNode*> stack{ m_scene->mRootNode };
        while (!stack.empty()) {
            const aiNode* n = stack.back();
            stack.pop_back();
            if (std::string(n->mName.C_Str()).find("_$AssimpFbx$_Translation") != std::string::npos) {
                m_usesFbxRrTtHelpers = true;
                break;
            }
            for (unsigned int c = 0; c < n->mNumChildren; c++)
                stack.push_back(n->mChildren[c]);
        }
        // Diagnostic one-shot : le mode RrTt change la facon dont les canaux
        // sont appliques (rotation seule + translation bind). Le savoir au
        // lancement aide a interpreter les logs suivants.
        LOG_INFO("[Animator] Decomposition FBX RrTt (Assimp helpers): %s",
                 m_usesFbxRrTtHelpers ? "oui" : "non");
    }
}

void Animator::playAnimation(unsigned int animIndex, bool loop, bool crossfade) {
    const aiAnimation* anim = m_model ? m_model->getAnimation(animIndex) : nullptr;
    if (!anim) {
        LOG_ERROR("[Animator] Animation index %u invalide", animIndex);
        return;
    }
    playAnimation(anim, loop, crossfade);
}

void Animator::playAnimation(const aiAnimation* newAnim, bool loop, bool crossfade) {
    if (!newAnim) {
        LOG_ERROR("[Animator] Animation nullptr");
        return;
    }

    // Crossfade : on prend un instantané des transformées LOCALES de la
    // dernière frame rendue (stockées dans m_currentLocalTransforms par
    // computeBoneTransform). Le blend se fera dans computeBoneTransform()
    // en slerpant/lerpant les transforms locales, PUIS en recomposant la
    // hiérarchie → translation et rotation restent synchrones, pas d'overshoot.
    if (crossfade && m_currentAnimation && m_currentAnimation != newAnim) {
        m_prevLocalTransforms = m_currentLocalTransforms;
        m_fadeTimer = 0.0f;
        m_crossfading = true;
    } else {
        m_crossfading = false;
    }

    m_currentAnimation = newAnim;
    m_currentAnimName = m_currentAnimation->mName.C_Str();
    m_currentTime = 0.0f;

    // Assimp peut rapporter 0.0 (ou une valeur aberrante) : fallback 25 t/s.
    // Pour le GLB, on conserve impérativement la cadence fournie par Assimp,
    // car Assimp peut avoir converti les temps glTF en ticks internes.
    const double ticks = m_currentAnimation->mTicksPerSecond;
    m_ticksPerSecond = (ticks > 0.001) ? static_cast<float>(ticks) : 25.0f;

    // Assimp 5.4.3 corrompt certaines clés de rotation à l'import. Le
    // nettoyage est effectué une seule fois par animation : playAnimation()
    // peut être appelé plusieurs fois lors des transitions fire -> idle.
    if (m_repairedAnimations.insert(m_currentAnimation).second) {
        repairRotationKeys(m_currentAnimation);
    }

    // Cache des canaux par nom : évite la recherche linéaire à chaque frame.
    // Enregistre aussi la variante NORMALISEE (prefixe Mixamo retire + suffixe
    // "_$AssimpFbx$_Rotation" retire) pour que les canaux des animations FBX
    // externes matchent les bones du modele, meme avec des prefixes differents
    // ("mixamorig:" vs "mixamorig2:") et des canaux RrTt ("LeftArm_$AssimpFbx$_Rotation").
    m_channelMap.clear();
    for (unsigned int i = 0; i < m_currentAnimation->mNumChannels; i++) {
        const aiNodeAnim* channel = m_currentAnimation->mChannels[i];
        if (!channel) continue;
        std::string name(channel->mNodeName.C_Str());
        m_channelMap[name] = channel;
        std::string normalized = normalizeNodeName(name);
        if (normalized != name) {
            m_channelMap[normalized] = channel;
        }
    }

    m_loop = loop;
    m_debugTimer = 0.0f;
    m_debugLastTime = 0.0f;
    m_debugHasLastRotation = false;

    // Diagnostic one-shot au premier play (uniquement si aucun channel ne matche)
    if (m_debugPrintedAnimations.insert(m_currentAnimation).second) {
        const auto& boneMap = m_model->getBoneInfoMap();
        // Un canal matche si son nom NORMALISE (prefixe Mixamo + suffixe
        // Assimp FBX retires) correspond a celui d'un bone du modele.
        unsigned int matched = 0;
        for (unsigned int i = 0; i < m_currentAnimation->mNumChannels; i++) {
            const aiNodeAnim* ch = m_currentAnimation->mChannels[i];
            if (!ch) continue;
            const std::string norm = normalizeNodeName(ch->mNodeName.C_Str());
            for (const auto& pair : boneMap) {
                if (normalizeNodeName(pair.first) == norm) { matched++; break; }
            }
        }
        if (matched == 0 && m_currentAnimation->mNumChannels > 0) {
            LOG_WARN("[Animator] %s: 0/%u channels matchent !",
                     m_currentAnimName.c_str(), m_currentAnimation->mNumChannels);
        }
    }
}

void Animator::printAnimationDebug() const {
    if (!m_currentAnimation) return;

    unsigned int rotationChannels = 0;
    unsigned int rotationKeys = 0;
    unsigned int positionChannels = 0;
    unsigned int positionKeys = 0;
    double minRotationTime = DBL_MAX;
    double maxRotationTime = -DBL_MAX;    const aiNodeAnim* upperArm = nullptr;


    for (unsigned int i = 0; i < m_currentAnimation->mNumChannels; i++) {
        const aiNodeAnim* channel = m_currentAnimation->mChannels[i];
        if (!channel) continue;
        if (channel->mNumRotationKeys > 0) {
            rotationChannels++;
            rotationKeys += channel->mNumRotationKeys;
            minRotationTime = std::min(minRotationTime, channel->mRotationKeys[0].mTime);
            maxRotationTime = std::max(maxRotationTime,
                channel->mRotationKeys[channel->mNumRotationKeys - 1].mTime);
        }
        if (channel->mNumPositionKeys > 0) {
            positionKeys += channel->mNumPositionKeys;
        }
        if (channel->mNodeName == aiString("upper_arm.R")) {
            upperArm = channel;
        }
    }


    if (upperArm && upperArm->mNumRotationKeys > 0) {
        const aiQuatKey& first = upperArm->mRotationKeys[0];
        const aiQuatKey& last = upperArm->mRotationKeys[upperArm->mNumRotationKeys - 1];
        const float firstLen = glm::length(glm::quat(first.mValue.w, first.mValue.x,
                                                       first.mValue.y, first.mValue.z));
        const float lastLen = glm::length(glm::quat(last.mValue.w, last.mValue.x,
                                                      last.mValue.y, last.mValue.z));
    }
}

void Animator::repairRotationKeys(const aiAnimation* anim) {
    if (!anim) return;
    const float duration = static_cast<float>(anim->mDuration);
    unsigned int repairedChannels = 0;
    unsigned int repairedKeys = 0;

    for (unsigned int c = 0; c < anim->mNumChannels; c++) {
        aiNodeAnim* ch = anim->mChannels[c];
        if (!ch) {
            LOG_WARN("[Animator] Canal d'animation null a l'index %u", c);
            continue;
        }
        const unsigned int n = ch->mNumRotationKeys;
        if (n == 0) continue;

        // Assimp peut fournir les clés GLB dans un ordre non monotone. Sans
        // tri, une clé tardive fait monter lastTime et toutes les clés
        // suivantes sont rejetées, ce qui explique par exemple 15 clés au
        // lieu de 60 pour upper_arm.R. Les temps non finis sont envoyés à la
        // fin et seront rejetés par timeOk.
        std::sort(ch->mRotationKeys, ch->mRotationKeys + n,
            [](const aiQuatKey& a, const aiQuatKey& b) {
                const bool aFinite = std::isfinite(a.mTime);
                const bool bFinite = std::isfinite(b.mTime);
                if (aFinite != bFinite) return aFinite;
                if (!aFinite) return false;
                return a.mTime < b.mTime;
            });

        // Conserver les clés valides : temps fini dans [0, durée], quaternion
        // non nul et fini. Les clés corrompues sont écartées et les bonnes
        // compactées en tête du tableau.
        unsigned int kept = 0;
        unsigned int rejectedTime = 0;
        unsigned int rejectedQuaternion = 0;
        float lastTime = -FLT_MAX;
        for (unsigned int i = 0; i < n; i++) {
            aiQuatKey& k = ch->mRotationKeys[i];
            const float t = static_cast<float>(k.mTime);
            const glm::quat q(k.mValue.w, k.mValue.x, k.mValue.y, k.mValue.z);
            const float len = glm::length(q);
            const bool timeOk = std::isfinite(t) && t >= 0.0f && t <= duration + 1.0f && t >= lastTime;
            // Ne pas exiger une norme comprise entre 0.9 et 1.1 : certains
            // exports fournissent des quaternions valides mais non normalisés.
            // Les supprimer réduit fortement la fréquence des poses et rend
            // l'animation saccadée. On garde donc toute rotation finie,
            // non-nulle et raisonnable, puis on la normalise immédiatement.
            // Les valeurs manifestement corrompues (ex: ~1e17) restent rejetées.
            const bool quatOk = std::isfinite(len) && len > 0.0001f && len < 10.0f;
            if (timeOk && quatOk) {
                if (kept != i) ch->mRotationKeys[kept] = k;
                aiQuatKey& keptKey = ch->mRotationKeys[kept];
                keptKey.mValue.w /= len;
                keptKey.mValue.x /= len;
                keptKey.mValue.y /= len;
                keptKey.mValue.z /= len;
                kept++;
                lastTime = t;
            } else if (!timeOk) {
                rejectedTime++;
            } else {
                rejectedQuaternion++;
            }
        }

        if (kept != n) {
            repairedChannels++;
            repairedKeys += n - kept;
            if (rejectedTime > 0 || rejectedQuaternion > 0) {
            }
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
    }
}

void Animator::update(float deltaTime) {
    if (!m_currentAnimation || !m_scene || !m_model) return;

    // Un delta invalide peut contaminer le temps puis toutes les matrices.
    if (!std::isfinite(deltaTime) || deltaTime < 0.0f) {
        deltaTime = 0.0f;
    }
    m_currentTime += deltaTime * m_ticksPerSecond;

    const float duration = static_cast<float>(m_currentAnimation->mDuration);
    // Une animation vide ne doit jamais alimenter fmodf(..., 0) : cela
    // fabriquerait un temps NaN et rendrait toutes les interpolations instables.
    if (!std::isfinite(duration) || duration <= 0.0f) {
        m_currentTime = 0.0f;
    } else if (m_loop) {
        m_currentTime = std::fmod(m_currentTime, duration);
    } else if (m_currentTime > duration) {
        m_currentTime = duration;
    }

    // Progression périodique après normalisation du temps : permet de voir si
    // le temps avance réellement, si le cycle reboucle et si la pose change.
    m_debugTimer += deltaTime;
    if (m_debugTimer >= 1.0f) {
        m_debugTimer = 0.0f;
        const auto it = m_channelMap.find("upper_arm.R");
        if (it != m_channelMap.end()) {
            const aiNodeAnim* channel = it->second;
            const glm::quat identity(1.0f, 0.0f, 0.0f, 0.0f);
            const glm::quat rotation = interpolateRotation(m_currentTime, channel, identity);
            const float deltaRotation = m_debugHasLastRotation
                ? glm::length(rotation - m_debugLastRotation) : 0.0f;
            const bool looped = m_currentTime < m_debugLastTime;
            m_debugLastRotation = rotation;
            m_debugHasLastRotation = true;
        } else {
        }
        m_debugLastTime = m_currentTime;
    }

    // Progression du crossfade (si un fondu est en cours)
    if (m_crossfading) {
        m_fadeTimer += deltaTime;
        if (m_fadeTimer >= kCrossfadeDuration) {
            m_crossfading = false;
        }
    }
    // Facteur de blend stocké pour computeBoneTransform() : utilisé à
    // l'intérieur de la récursion pour blenders les transforms locales.
    m_fade = m_crossfading
        ? glm::smoothstep(0.0f, 1.0f, m_fadeTimer / kCrossfadeDuration)
        : 1.0f;

    // Partir du RootNode d'Assimp (contient l'échelle FBX, souvent ×0.01)
    // et non du nœud "root" du squelette, sinon les matrices bone
    // manquent la transformation globale du fichier → échelle ×100 → clipping.
    if (m_model->getRootNode()) {
        // Pose de l'animation courante (le crossfade est intégré DANS la
        // récursion : computeBoneTransform() blend les transforms locales
        // avant de recomposer la hiérarchie → plus de blend sur matrices finales)
        computeBoneTransform(m_model->getRootNode(), glm::mat4(1.0f), m_currentTime);
    }
}

void Animator::computeBoneTransform(const aiNode* node, const glm::mat4& parentTransform, float animTime) {
    std::string nodeName(node->mName.C_Str());
    const auto& boneMap = m_model->getBoneInfoMap();

    // Un nœud n'est un bone que s'il appartient au squelette CONSERVÉ
    // (ensemble de joints collecté au chargement). Le nom seul ne suffit pas :
    // human_1.glb contient MaleArm ET FemaleArm avec des noms de bones
    // identiques (COG, Hip...) — les nœuds du second rig (visités après le
    // premier) écraseraient les bone matrices du premier (même boneId attribué
    // par nom) et déformeraient le personnage conservé. Fallback sur la
    // recherche par nom si l'ensemble est vide (modèles sans infos joints).
    const auto& jointNodes = m_model->getJointNodes();
    bool isBone = jointNodes.empty()
        ? (boneMap.find(nodeName) != boneMap.end())
        : (jointNodes.find(node) != jointNodes.end());

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
    if (chanIt == m_channelMap.end()) {
        // Essayer avec le nom normalisé (prefixe Mixamo + suffixe Assimp FBX
        // retires) : couvre "mixamorig2:LeftArm" -> canal "mixamorig:LeftArm_$AssimpFbx$_Rotation".
        std::string normalized = normalizeNodeName(nodeName);
        if (normalized != nodeName) {
            chanIt = m_channelMap.find(normalized);
        }
    }
    if (chanIt != m_channelMap.end()) {
        nodeTransform = interpolateNodeTransform(node, chanIt->second, animTime);

        // ── Root motion lock : figer la translation du bone racine ─────
        // Les animations Mixamo contiennent un deplacement du Hips (root
        // motion). Comme la position du modele est deja geree par le jeu
        // (setPosition du joueur), on annule cette translation.
        //
        // Pour les animations one-shot (turn) : on zero aussi la rotation
        // Y du Hips, car le turn contient une rotation 90° qui s'ajoute
        // a celle de getModelMatrix() (setDirection) → double rotation.
        static const std::string kRootBoneName = "mixamorig:Hips";
        if (nodeName == kRootBoneName || stripMixamoPrefix(nodeName) == "Hips") {
            const glm::vec3 bindTrans = aiMatrixToGlm(node->mTransformation)[3];
            nodeTransform[3] = glm::vec4(bindTrans, 1.0f);

            if (!m_loop) {
                // One-shot : zero la rotation Y (turn 90° integre)
                glm::quat q = glm::quat_cast(glm::mat3(nodeTransform));
                q.y = 0.0f;
                float len = glm::length(q);
                q = (len > 0.0001f) ? glm::normalize(q)
                                    : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                glm::vec3 trans(nodeTransform[3]);
                nodeTransform = glm::mat4_cast(q);
                nodeTransform[3] = glm::vec4(trans, 1.0f);
            }
        }
    }

    // ── Crossfade : blender les transforms LOCALES (T·R pur) ─────────────
    // Stocker la transformée locale courante (servira d'instantané si un
    // fondu est déclenché plus tard). Clé = nœud (pas nom) pour isoler les
    // squelettes homonymes (voir Animator.h).
    m_currentLocalTransforms[node] = nodeTransform;

    // Pendant un fondu, blender la transformée locale figée (instantané)
    // vers la transformée courante de la nouvelle animation. Le blend se
    // fait SUR DU T·R PUR (pas de contamination par l'offsetMatrix) → la
    // translation et la rotation évoluent de façon synchrone, les doigts
    // ne peuvent pas overshoot.
    if (m_crossfading && m_fade < 1.0f) {
        auto prevIt = m_prevLocalTransforms.find(node);
        if (prevIt != m_prevLocalTransforms.end()) {
            nodeTransform = blendLocalTransforms(prevIt->second, nodeTransform, m_fade);
        }
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

glm::mat4 Animator::blendLocalTransforms(const glm::mat4& from, const glm::mat4& to, float t) {
    // Blend de deux transformées LOCALES (T·R pur, relatif au parent).
    // Contrairement à l'ancien mixBoneMatrices qui travaillait sur les
    // matrices finales (globalTransform * offsetMatrix → colonne 3
    // contaminée), ici from/to sont garanties T·R pur (pas d'offsetMatrix,
    // pas d'échelle) → lerp translation + slerp rotation sont SYNCHRONES
    // et ne peuvent pas overshoot les doigts.
    if (t >= 1.0f) return to;
    if (t <= 0.0f) return from;

    // Translation : colonne 3 (position locale du nœud)
    const glm::vec3 transFrom(from[3]);
    const glm::vec3 transTo(to[3]);
    const glm::vec3 transBlend = glm::mix(transFrom, transTo, t);

    // Rotation : extraire le quaternion de la partie 3×3
    const glm::quat rotFrom = glm::quat_cast(glm::mat3(from));
    const glm::quat rotTo   = glm::quat_cast(glm::mat3(to));

    // Chemin court (slerp) avec garde anti-singularité (dot ≈ 1 → nlerp)
    float dot = glm::dot(rotFrom, rotTo);
    glm::quat rotToAdj = rotTo;
    if (dot < 0.0f) {
        rotToAdj = -rotTo;
        dot = -dot;
    }
    glm::quat rotBlend;
    if (dot > 0.9999f) {
        rotBlend = glm::normalize(glm::mix(rotFrom, rotToAdj, t));
    } else {
        rotBlend = glm::slerp(rotFrom, rotToAdj, t);
    }

    // Garde anti-NaN : si le blend produit des valeurs invalides, on
    // bascule franchement sur la source ou la cible. Le return anticipé
    // évite que la correction de rotation (rotBlend) soit écrasée par
    // un éventuel fallback de translation.
    const bool rotFinite = std::isfinite(rotBlend.w) && std::isfinite(rotBlend.x) &&
                           std::isfinite(rotBlend.y) && std::isfinite(rotBlend.z);
    const bool transFinite = std::isfinite(transBlend.x) && std::isfinite(transBlend.y) &&
                              std::isfinite(transBlend.z);
    if (!rotFinite || !transFinite) {
        return (t < 0.5f) ? from : to;
    }

    const glm::mat4 T = glm::translate(glm::mat4(1.0f), transBlend);
    const glm::mat4 R = glm::mat4_cast(rotBlend);
    return T * R;
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

    glm::vec3 translation;
    if (m_usesFbxRrTtHelpers) {
        // FBX RrTt : la position est portee par le nœud wrapper
        // "bone_$AssimpFbx$_Translation" (bind). Appliquer la translation du
        // canal (ex: 0,55.13,0 pour l'avant-bras) en PLUS de celle du wrapper
        // (0,25.51,0) etirait le segment (~2x). On garde donc la translation
        // de bind pose : le canal ne fournit que la rotation.
        translation = bindTranslation;
    } else {
        translation = interpolateTranslation(animTime, channel, bindTranslation);
    }
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

        // En boucle, la dernière clé doit être interpolée vers la première.
        // Comme la première clé appartient au cycle suivant, on ajoute la
        // durée de l'animation à son temps (ex: 95 -> 0 devient 95 -> 100),
        // au lieu de figer la pose jusqu'au prochain reset de m_currentTime.
        if (nextIdx >= channel->mNumPositionKeys) {
            if (!m_loop) {
                const aiVector3D& v = channel->mPositionKeys[idx].mValue;
                const glm::vec3 last(v.x, v.y, v.z);
                if (!std::isfinite(last.x) || !std::isfinite(last.y) || !std::isfinite(last.z)) {
                    if (std::isfinite(defaultValue.x) && std::isfinite(defaultValue.y) && std::isfinite(defaultValue.z)) {
                        return defaultValue;
                    }
                    return glm::vec3(0.0f);
                }
                return last;
            }
            nextIdx = 0;
        }

        float t0 = static_cast<float>(channel->mPositionKeys[idx].mTime);
        float t1 = static_cast<float>(channel->mPositionKeys[nextIdx].mTime);
        if (nextIdx == 0 && m_loop) {
            t1 += static_cast<float>(m_currentAnimation->mDuration);
        }
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

        // En boucle, interpoler la dernière rotation vers la première sur
        // l'intervalle restant de l'animation. Sans ce wrap temporel, le code
        // restait bloqué sur la dernière clé puis sautait à la première.
        if (nextIdx >= channel->mNumRotationKeys) {
            if (!m_loop) {
                const aiQuaternion& q = channel->mRotationKeys[idx].mValue;
                const glm::quat res(q.w, q.x, q.y, q.z);
                const float rl = glm::length(res);
                return (std::isfinite(rl) && rl > 0.0001f) ? glm::normalize(res) : defaultValue;
            }
            nextIdx = 0;
        }

        float t0 = static_cast<float>(channel->mRotationKeys[idx].mTime);
        float t1 = static_cast<float>(channel->mRotationKeys[nextIdx].mTime);
        if (nextIdx == 0 && m_loop) {
            t1 += static_cast<float>(m_currentAnimation->mDuration);
        }
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
            // Quaternions (quasi-)identiques : on NE fige PAS sur la clé de
            // début de segment (ancien comportement : pose constante puis
            // saut à la clé suivante → escalier "hold & jump"). Sur
            // finger_gun_fire, les derniers segments de hand.R/forearm.R/
            // upper_arm.R sont < 1.6° → la fin du tir s'arrêtait 2 frames
            // puis sautait en revenant à l'idle (saccade/lag visible sur les
            // doigts). Un nlerp (mix + normalisation) reste lisse et sans
            // NaN : pas de division par sin(θ).
            result = glm::normalize(glm::mix(a, b, factor));
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

    // Même fermeture cyclique pour l'échelle. Cette fonction n'est pas
    // utilisée actuellement (l'échelle d'animation est ignorée), mais garder
    // les trois types cohérents évite une future saccade de fin de cycle.
    if (nextIdx >= channel->mNumScalingKeys) {
        if (!m_loop) {
            const aiVector3D& s = channel->mScalingKeys[idx].mValue;
            const glm::vec3 last(s.x, s.y, s.z);
            return (std::isfinite(last.x) && std::isfinite(last.y) && std::isfinite(last.z))
                   ? last : glm::vec3(1.0f);
        }
        nextIdx = 0;
    }

    float t0 = static_cast<float>(channel->mScalingKeys[idx].mTime);
    float t1 = static_cast<float>(channel->mScalingKeys[nextIdx].mTime);
    if (nextIdx == 0 && m_loop) {
        t1 += static_cast<float>(m_currentAnimation->mDuration);
    }
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
