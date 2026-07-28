#include "ArmsRenderer.h"

#include "Camera.h"
#include "Shader.h"
#include "ShaderManager.h"
#include "LightManager.h"
#include "Model.h"
#include "Animator.h"
#include "Mesh.h"        // pour mesh->draw() sur Model::getMeshes()
#include "constants.h"

#include <glad/glad.h>  // GL_DEPTH_TEST, GL_DEPTH_BUFFER_BIT, glDisable/glClear
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cmath>

void ArmsRenderer::drawFP(Camera* camera,
                          ShaderManager* shaderManager,
                          LightManager* lightManager,
                          Model* armsModel,
                          Animator* armsAnimator) {
    if (!camera || !shaderManager || !armsModel || !armsAnimator) return;

    // 0. Securite : shader skinned compile-t-il ?
    Shader* skinned = nullptr;
    try { skinned = shaderManager->getShader("skinned"); }
    catch (const std::exception& e) {
        std::cerr << "[ArmsRenderer] skinned shader indisponible : " << e.what() << std::endl;
        return;
    }
    if (!skinned) return;

    skinned->use();

    // 0.b Samplers : le shader skinned declare uniform sampler2D texture_diffuse
    //    et texture_specular (cf. skinned.frag). Mesh::draw() bind les textures
    //    aux unites GL_TEXTURE0 (diffuse) et GL_TEXTURE1 (speculaire), mais le
    //    shader a besoin de savoir que texture_diffuse=0 et texture_specular=1.
    //    Model::draw() le fait ; ArmsRenderer doit faire de meme, sinon les
    //    samplers pointent vers l'unité 0 par defaut pour les deux -> la
    //    speculaire ecrase la diffuse (ou l'inverse) -> rendu incorrect.
    skinned->setInt("texture_diffuse", 0);
    skinned->setInt("texture_specular", 1);

    // 1. Matrice model camera-relative : FP_ARMS_DOWN_OFFSET sous les yeux,
    //    FP_ARMS_FORWARD_OFFSET devant, yaw camera + FP_ARMS_YAW_OFFSET,
    //    scale global via FP_ARMS_SCALE (voir constants.h).
    glm::vec3 eyePos = camera->getPosition();
    glm::vec3 front   = camera->getFront();
    glm::vec3 up      = camera->getUp();

    // 1.a Dynamic safe-forward push : FP_ARMS_FORWARD_OFFSET est code en dur,
    // mais l'extent Z du mesh depend de l'asset charge. Si une vertice tombe
    // a l'interieur de la zone (0, nearPlane), OpenGL la clippe -> artefacts
    // rouges (Z-fighting / flickering). On calcule donc un offset safe :
    //
    //     safeForward = max(FP_ARMS_FORWARD_OFFSET,
    //                       nearPlane + scale*(maxDist*FP_ARMS_ANIM_EXPANSION) + margin)
    //
    // ou maxDist = distance euclidienne 3D max depuis l'ORIGINE du rig sur
    // l'AABB locale aggregee de tous les meshes. On ne prend PAS seulement Z :
    // un rig dont l'origine est au pied (Y=0) avec les bras a Y=1.66 a
    // maxAbsZ=0.14 mais maxDist=1.86. Si on ignore Y, les vertices hautes
    // franchissent le near-plane des que la camera pivote en pitch -> fragments
    // clippes -> clignotement marron.
    //
    // Exemple : pour le rig arms_rig.glb, maxDist = 1.86, scale = 0.25 et
    // FP_ARMS_ANIM_EXPANSION = 1.5, l'extent reel vers la camera =
    // 1.86 * 1.5 * 0.25 = 0.70 ; il faut safeForward >= 0.1 + 0.70 + 0.05 = 0.85 ;
    // avec FP_ARMS_FORWARD_OFFSET = 0.45, on pousse a 0.85 automatiquement
    // (sans cap — l'ancien plafond 0.7m aurait bloque a 0.7 et laisse les
    // vertices franchir le near-plane -> clignotement).
    //
    // Cache : safeForwardOffset est invariant tant que armsModel ne change pas
    // (mesh AABB calculee une seule fois au load). On evite ainsi de refaire
    // l'agregat O(N_meshes) + ses op glm chaque frame. Re-derive si le
    // Model* a change (ex : reload du rig a chaud).
    constexpr float kNearPlane       = 0.1f;
    constexpr float kNearPlaneMargin = 0.05f;  // 5 cm : evite le Z-fighting pile au near plane
    static const Model* sCachedModel = nullptr;
    static float        sCachedSafeForward = Constants::FP_ARMS_FORWARD_OFFSET;
    if (sCachedModel != armsModel) {
        sCachedModel = armsModel;
        // Fallback safe : sans meshes, on garde la constante d'origine.
        sCachedSafeForward = Constants::FP_ARMS_FORWARD_OFFSET;
        const auto& meshes = armsModel->getMeshes();
        if (!meshes.empty()) {
            // Seed depuis le premier mesh, etend aux suivants.
            // Pas besoin de sentinelle +/-infinity (correction reviewer).
            // Garde null : si le premier mesh est nullptr (cas pathologique),
            // on garde l'offset par defaut (FP_ARMS_FORWARD_OFFSET deja set ci-
            // dessus) plutot que de crasher sur first->getLocalAABBMin().
            const Mesh* first = meshes.front();
            if (first) {
                glm::vec3 localMin = first->getLocalAABBMin();
                glm::vec3 localMax = first->getLocalAABBMax();
                for (size_t i = 1; i < meshes.size(); ++i) {
                    const Mesh* m = meshes[i];
                    if (!m) continue;
                    localMin = glm::min(localMin, m->getLocalAABBMin());
                    localMax = glm::max(localMax, m->getLocalAABBMax());
                }
                // Distance 3D max depuis l'ORIGINE du rig (pas seulement Z).
                // Un rig dont l'origine est au pied (Y=0) avec les bras a
                // Y=1.66 a maxAbsZ=0.14 mais maxDist=1.86. Si on ne considere
                // que Z (comme l'ancien code), les vertices hautes franchissent
                // le near-plane des que la camera pivote en pitch -> fragments
                // clippes -> clignotement marron (texture skin eclairée par la
                // DirLight tamisée). On prend donc la distance euclidienne
                // complete max(|X|,|Y|,|Z|) pour couvrir toutes les directions
                // de rotation camera.
                const float maxAbsX = std::max(std::abs(localMin.x), std::abs(localMax.x));
                const float maxAbsY = std::max(std::abs(localMin.y), std::abs(localMax.y));
                const float maxAbsZ = std::max(std::abs(localMin.z), std::abs(localMax.z));
                const float maxDistFromOrigin = std::sqrt(
                    maxAbsX * maxAbsX + maxAbsY * maxAbsY + maxAbsZ * maxAbsZ);
                // Offset dynamique SANS cap : l'ancien plafond
                // FP_ARMS_MAX_SAFE_FORWARD (0.7m) causait precisement le
                // clignotement rouge en interdisant a l'offset de suivre
                // l'extent reel des rigs animes. L'aspect "3e personne" se
                // regle cote asset (rig d'avant-bras seul) plutot que par un
                // cap qui sacrifie la robustesse du rendu.
                //
                // Expansion : l'AABB locale est mesuree sur le BIND POSE (vertices
                // au repos), mais pendant l'animation les vertices s'etendent au-
                // dela (mains qui pivotent vers la camera, coudes qui montent). On
                // applique donc FP_ARMS_ANIM_EXPANSION (> 1.0) sur la distance 3D
                // pour couvrir la plupart des animations. NB : c'est une mitigation
                // heuristique, pas une garantie absolue — une animation extreme
                // (bras tendu en avant, swing d'arme) peut encore depasser 1.5x
                // l'extent bind-pose. La solution robuste serait de recalculer
                // l'AABB skinnee par frame (cout CPU eleve), intentionnellement
                // reportee. Sans cette marge, les vertices qui franchissent
                // [0, nearPlane] sont clippees par le GPU et produisent le
                // clignotement (Z-fight + interpolation garbage sur les
                // normales/coords des fragments clippes).
                const float expandedExt = Constants::FP_ARMS_SCALE
                                        * (maxDistFromOrigin * Constants::FP_ARMS_ANIM_EXPANSION);
                sCachedSafeForward = std::max(
                    Constants::FP_ARMS_FORWARD_OFFSET,
                    kNearPlane + expandedExt + kNearPlaneMargin
                );
                // Garde-fou runtime : sans le cap supprime, un rig full-body (au
                // lieu d'un rig d'avant-bras seul) pousse l'offset a plusieurs
                // metres -> mannequin flottant en "3e personne". On log un
                // avertissement pour qu'un mauvais asset ne passe pas inapercu.
                // L'aspect 3e personne se regle cote asset, pas par un cap qui
                // sacrifierait la robustesse du rendu sur les rigs d'avant-bras
                // corrects.
                if (sCachedSafeForward > 2.0f * Constants::FP_ARMS_DOWN_OFFSET) {
                    std::cerr << "[ArmsRenderer] ATTENTION : safeForwardOffset=" << sCachedSafeForward
                              << "m (trop grand) — l'asset semble etre un rig full-body,"
                              << " pas un rig d'avant-bras. Rendu en vue 3e personne probable."
                              << std::endl;
                }
            } // ferme if (first)
        } // ferme if (!meshes.empty())
    } // ferme if (sCachedModel != armsModel)
    const float safeForwardOffset = sCachedSafeForward;

    glm::mat4 armsModelMat(1.0f);
    armsModelMat = glm::translate(armsModelMat,
        eyePos - up * Constants::FP_ARMS_DOWN_OFFSET
              + front * safeForwardOffset);

    glm::vec3 flatFront = glm::normalize(glm::vec3(front.x, 0.0f, front.z));
    if (glm::length(flatFront) > 1e-4f) {
        float yaw = std::atan2(flatFront.x, flatFront.z) + Constants::FP_ARMS_YAW_OFFSET;
        armsModelMat = glm::rotate(armsModelMat, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    }
    armsModelMat = glm::scale(armsModelMat, glm::vec3(Constants::FP_ARMS_SCALE));

    // 2. setupMatrices() : envoie uModel (= armsModelMat, setter juste avant),
    //    uView (= m_camera->getViewMatrix() du shader, frais par frame) et
    //    uProjection (= m_projection du shader, partagee avec les autres
    //    shader 3D -> coherence visuelle avec le decor). On evite ainsi la
    //    duplication d'une glm::perspective locale.
    skinned->setModel(armsModelMat);
    skinned->setupMatrices();

    // 3. Bone palette : on pad avec identite jusqu'a MAX_BONES (100, voir
    //    skinned.vert) -> evite toute lecture non-initialisee cote GPU si
    //    l'asset expose moins de 100 bones. count = clamp(0, bones.size, 100).
    const std::vector<glm::mat4>& bones = armsAnimator->getFinalBoneMatrices();
    glm::mat4 padded[100];
    int count = std::max(0, std::min(static_cast<int>(bones.size()), 100));
    for (int i = 0; i < count; ++i) padded[i] = bones[i];
    for (int i = count; i < 100; ++i) padded[i] = glm::mat4(1.0f);
    skinned->setMat4Array("uBoneMatrices", padded, 100);

    // 4. Lumieres : on suit la meme voie que Model::draw (cf. model shader
    //    draw path) pour que les bras soient eclaires identiquement au decor.
    skinned->setVec3("viewPos", eyePos);
    if (lightManager) {
        lightManager->applyToShader(skinned);
    }
    else {
        skinned->setInt("numLights", 0);
    }

    // 5. Clear depth + draw direct de chaque mesh. Mesh::draw() ne re-bind PAS
    //    de programme shader (il bind juste VAO + textures + (re)active les
    //    attribs skin dans le VAO), donc le programme skinned reste actif
    //    pendant la boucle -> les uBoneMatrices deja uploadees ci-dessus
    //    sont toujours visibles cote GPU.
    glDisable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
    for (auto* mesh : armsModel->getMeshes()) {
        mesh->draw();
    }
    glEnable(GL_DEPTH_TEST);
}
