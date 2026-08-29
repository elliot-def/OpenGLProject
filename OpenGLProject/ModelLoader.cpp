#include "win_compat.h"

#include "ModelLoader.h"
#include "ModelEntity.h"
#include "FirstPersonArms.h"
#include "CharacterAnimationController.h"
#include "Log.h"
#include "InputManager.h"
#include "Animator.h"
#include "constants/resource.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <cstdio>
#include <string>
#include <vector>

namespace {
// Diagnostic RrTt : dump l'arbre des noeuds (hierarchie + bind) pour les bones
// du torse/jambes/tete afin d'identifier la structure de decomposition Assimp
// (wrappers "_$AssimpFbx$_Translation" / "_PreRotation" autour de chaque bone).
void dumpNodeTree(const aiNode* node, int depth) {
    if (!node || depth > 10) return;
    const std::string name = node->mName.C_Str();
    const bool relevant =
        name.find("Hip") != std::string::npos ||
        name.find("Leg") != std::string::npos ||
        name.find("Foot") != std::string::npos ||
        name.find("Spine") != std::string::npos ||
        name.find("Neck") != std::string::npos ||
        name.find("Head") != std::string::npos;
    if (relevant) {
        const glm::mat4 m = aiMatrixToGlm(node->mTransformation);
        const glm::vec3 t = glm::vec3(m[3]);
        LOG_INFO("[ModelLoader]   %*s'%s' bind=(%.2f, %.2f, %.2f) diag=(%.2f, %.2f, %.2f)",
                 depth * 2, "", name.c_str(), t.x, t.y, t.z,
                 m[0][0], m[1][1], m[2][2]);
    }
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        dumpNodeTree(node->mChildren[i], depth + 1);
    }
}
} // namespace

ModelLoader::ModelLoader(Camera* camera, LightManager* lightManager, Renderer* renderer,
                         TextureManager* textureManager, InputManager* inputManager)
    : m_camera(camera), m_lightManager(lightManager), m_renderer(renderer),
      m_textureManager(textureManager), m_inputManager(inputManager) {}

ModelLoader::~ModelLoader() = default;

// ---------------------------------------------------------------------------
// Chargement asynchrone des modèles 3D (exécuté sur un thread separe)
// ---------------------------------------------------------------------------

void ModelLoader::load(GLFWwindow* loaderWindow) {
    if (loaderWindow)
        glfwMakeContextCurrent(loaderWindow);

    loadDecorModels();
    loadHumanCharacter();

    if (loaderWindow)
        glfwMakeContextCurrent(nullptr);
}

void ModelLoader::loadDecorModels() {
    LOG_INFO("[ModelLoader]   -> Chargement du backpack...");
    m_modelEntity = std::make_unique<ModelEntity>(m_camera, m_lightManager, m_renderer,
                                                  Constants::Resource::MODEL_BACKPACK, m_textureManager);

    LOG_INFO("[ModelLoader]   -> Chargement de fropy (low poly)...");
    m_fropyEntity = std::make_unique<ModelEntity>(m_camera, m_lightManager, m_renderer,
                                                  Constants::Resource::MODEL_FROPY, m_textureManager);
    m_fropyEntity->setPosition(glm::vec3(3.0f, 5.0f, 0.0f));
    m_fropyEntity->setSpin(20.0f, glm::vec3(0.0f, 1.0f, 0.0f));

    LOG_INFO("[ModelLoader]   -> Chargement des bras (rigges)...");
    m_firstPersonArms = std::make_unique<FirstPersonArms>(m_camera, m_lightManager,
                                                          Constants::Resource::MODEL_ARMS_RIG, m_textureManager);
    m_inputManager->setFirstPersonArms(m_firstPersonArms.get());
}

void ModelLoader::loadHumanCharacter() {
    LOG_INFO("[ModelLoader]   -> Chargement de Megan (rigge)...");
    m_humanEntity = std::make_unique<ModelEntity>(m_camera, m_lightManager, m_renderer,
                                                  Constants::Resource::MODEL_MEGAN, m_textureManager);

    Model* model = m_humanEntity->getModel();
    const aiScene* scene = model->getScene();

    // ── Debug chargement : un echec ici = aucun personnage en 3P ────────
    if (!scene || model->getMeshes().empty()) {
        LOG_ERROR("[ModelLoader]   !! ERREUR : le modele n'a pas pu etre charge "
                  "(scene=%p, meshes=%zu) -> rien ne sera affiche en 3P.",
                  (const void*)scene, model->getMeshes().size());
        LOG_ERROR("[ModelLoader]   !! Verifier que le fichier est lisible par Assimp "
                  "(FBX 2011/2012/2013 ou GLB/GLTF) : un ancien FBX 6.x est refuse.");
    } else {
        LOG_INFO("[ModelLoader]   Modele OK : %zu meshes, %u bones, %zu animations embarquees, "
                 "bbox size=(%.2f, %.2f, %.2f)",
                 model->getMeshes().size(),
                 static_cast<unsigned int>(model->getBoneInfoMap().size()),
                 model->getNumAnimations(),
                 model->getBoundingBox().getSize().x,
                 model->getBoundingBox().getSize().y,
                 model->getBoundingBox().getSize().z);
    }

    // Configuration commune du personnage Megan : auto-scale + animations
    // externes + mapping des index + idle. Partagee avec MultiplayerManager
    // pour creer les joueurs distants a l'identique.
    configureHumanCharacter(m_humanEntity.get());

    // Controleur d'animation du personnage 3P (extrait de Game::update)
    m_characterAnim = std::make_unique<CharacterAnimationController>(m_humanEntity.get(), m_inputManager);
}

// ---------------------------------------------------------------------------
// Configuration commune du personnage Megan (auto-scale + animations externes
// + mapping des index + idle). Reutilisee par le personnage local ET par les
// joueurs distants (MultiplayerManager) pour garantir un mapping identique.
// ---------------------------------------------------------------------------

void ModelLoader::configureHumanCharacter(ModelEntity* entity) {
    Model* model = entity->getModel();

    // Auto-scale : hauteur cible ~1.8 unites (~1.80m) divisee par la hauteur
    // reelle de la bounding box du modele (qui inclut le x100 FBX).
    {
        const float modelHeight = model->getBoundingBox().getSize().y;
        constexpr float targetHeight = 1.8f;
        if (modelHeight > 0.001f) {
            entity->setScale(targetHeight / modelHeight);
            LOG_INFO("[ModelLoader]   Megan auto-scale: %.4f (model=%.1f -> target=%.1f)",
                     targetHeight / modelHeight, modelHeight, targetHeight);
        } else {
            LOG_INFO("[ModelLoader]   Pas d'auto-scale (hauteur modele=%.4f, < 0.001)", modelHeight);
        }
    }

    // Charger les animations externes (FBX separes Mixamo)
    {
        const std::string animDir = Constants::Resource::MIXAMO_ANIM_DIR;
        std::vector<std::string> animPaths = {
            animDir + "idle.fbx",
            animDir + "walking.fbx",
            animDir + "standard run.fbx",
            animDir + "jump.fbx",
            animDir + "left strafe.fbx",
            animDir + "left strafe.fbx",  // strafe droit = miroir du gauche (voir plus bas)
            animDir + "left strafe walking.fbx",
            animDir + "right strafe walking.fbx",
            animDir + "left turn 90.fbx",
            animDir + "right turn 90.fbx",
            animDir + "Running Jump.fbx",
            animDir + "Walking Backwards.fbx",
            animDir + "Falling Idle.fbx",
            animDir + "Falling To Landing.fbx",
        };
        model->loadExternalAnimations(animPaths);

        // Toutes les animations Mixamo s'appellent "mixamo.com" -> impossible
        // de les distinguer par nom. On les identifie par leur ORDRE de
        // chargement (voir animPaths ci-dessus). Le mapping est DYNAMIQUE :
        // les clips externes s'ajoutent APRES les animations embarquees du
        // modele (ex: Remy.fbx embarque une T-pose a l'index 0 ; un modele
        // sans animation embarque commence directement a l'index 0).
        const size_t totalAnims = model->getNumAnimations();
        const size_t base = totalAnims - model->getNumExternalAnimations();

        entity->setIdleAnimIndex(static_cast<int>(base + 0));       // idle.fbx
        entity->setWalkAnimIndex(static_cast<int>(base + 1));       // walking.fbx
        entity->setRunAnimIndex(static_cast<int>(base + 2));        // standard run.fbx
        entity->setJumpIdx(static_cast<int>(base + 3));             // jump.fbx
        entity->setStrafeLeftIdx(static_cast<int>(base + 4));       // left strafe.fbx
        entity->setStrafeRightIdx(static_cast<int>(base + 5));      // left strafe.fbx MIRROIR (voir plus bas)
        entity->setStrafeWalkLeftIdx(static_cast<int>(base + 6));   // left strafe walking.fbx
        entity->setStrafeWalkRightIdx(static_cast<int>(base + 7));  // right strafe walking.fbx

        // ── Strafe droit = miroir du strafe gauche ─────────────────────
        // Les clips "right strafe.fbx" de Mixamo ne sont pas des miroirs
        // exacts des "left strafe.fbx" : le pied/la cheville droits y
        // pivotent ~115° a travers la direction de marche (le pied pointe
        // vers l'avant puis vers l'arriere pendant le cycle) alors que le
        // strafe gauche garde les pieds dans le plan lateral. On charge donc
        // le clip gauche DEUX fois et on miroirise la 2e copie : le strafe
        // droit devient l'exact miroir du gauche (meme comportement visuel,
        // symetrique). L'externe #5 correspond a l'index base+5 ci-dessus.
        model->mirrorExternalAnimation(5);
        entity->setTurnLeftIdx(static_cast<int>(base + 8));         // left turn 90.fbx
        entity->setTurnRightIdx(static_cast<int>(base + 9));        // right turn 90.fbx
        entity->setRunJumpIdx(static_cast<int>(base + 10));         // Running Jump.fbx
        entity->setWalkBackIdx(static_cast<int>(base + 11));        // Walking Backwards.fbx
        entity->setFallingIdleIdx(static_cast<int>(base + 12));     // Falling Idle.fbx
        entity->setFallingToLandingIdx(static_cast<int>(base + 13)); // Falling To Landing.fbx
        // punch = -1 (pas d'anim de punch)
        // rest  = -1 (pas d'anim de rest)

        // ── Debug : liste complete + validation du mapping ──────────────
        LOG_INFO("[ModelLoader]   %zu animations au total (%zu embarquees + %zu externes) :",
                 totalAnims, base, model->getNumExternalAnimations());
        for (size_t i = 0; i < totalAnims; i++) {
            const aiAnimation* anim = model->getAnimation(i);
            LOG_INFO("[ModelLoader]     [%2zu] \"%s\"%s", i,
                     anim ? anim->mName.C_Str() : "(null)",
                     i < base ? "  (embarquee)" : "  (externe)");
        }
        LOG_INFO("[ModelLoader]   Mapping : idle=%d walk=%d run=%d jump=%d strafeL=%d strafeR=%d "
                 "strafeWL=%d strafeWR=%d turnL=%d turnR=%d runJump=%d walkBack=%d "
                 "fallingIdle=%d fallingToLanding=%d",
                 entity->getIdleAnimIndex(), entity->getWalkAnimIndex(),
                 entity->getRunAnimIndex(), entity->getJumpIdx(),
                 entity->getStrafeLeftIdx(), entity->getStrafeRightIdx(),
                 entity->getStrafeWalkLeftIdx(), entity->getStrafeWalkRightIdx(),
                 entity->getTurnLeftIdx(), entity->getTurnRightIdx(),
                 entity->getRunJumpIdx(), entity->getWalkBackIdx(),
                 entity->getFallingIdleIdx(), entity->getFallingToLandingIdx());
        if (base + 13 >= totalAnims) {
            LOG_WARN("[ModelLoader]   !! ATTENTION : dernier clip externe (index %zu) >= nombre "
                     "d'animations (%zu) -> tous les clips n'existent pas.", base + 13, totalAnims);
        }

        // Jouer l'idle
        const int idleIdx = entity->getIdleAnimIndex();
        if (idleIdx >= 0 && idleIdx < static_cast<int>(totalAnims)) {
            entity->getAnimator()->playAnimation(static_cast<unsigned int>(idleIdx), true);
            entity->getAnimator()->update(0.0f);
        }
    }

    // ── Diagnostic skinning (à retirer une fois le bug identifié) ──────
    // Utile en solo comme en multi : confirme que les bones ont bien ete
    // importes et que la 1ere matrice de bone (souvent le Hips/root) est
    // valide (translation non nulle, diagonale ~1) et non identite/zero.
    {
        const Animator* anim = entity->getAnimator();
        const aiAnimation* cur = anim ? anim->getCurrentAnimation() : nullptr;
        LOG_INFO("[ModelLoader] Skinning pret : %zu bones, %zu animations, hasAnimations=%d, "
                 "scale=%.4f, anim='%s' (index=%d, loop=%d)",
                 model->getBoneInfoMap().size(), model->getNumAnimations(),
                 entity->hasAnimations() ? 1 : 0, entity->getScale(),
                 cur ? cur->mName.C_Str() : "(null)",
                 anim ? anim->getCurrentAnimationIndex() : -1,
                 (anim && anim->isLooping()) ? 1 : 0);
        const auto& bm = anim ? anim->getFinalBoneMatrices() : std::vector<glm::mat4>();
        if (!bm.empty()) {
            LOG_INFO("[ModelLoader]   bone[0] transl=(%.3f, %.3f, %.3f) diag=(%.3f, %.3f, %.3f, %.3f)",
                     bm[0][3].x, bm[0][3].y, bm[0][3].z,
                     bm[0][0][0], bm[0][1][1], bm[0][2][2], bm[0][3][3]);
        }
        // Positions monde (apres idle) : les jambes doivent avoir un Y
        // DECROISSANT (LeftUpLeg < Hips, LeftLeg < LeftUpLeg, LeftFoot < LeftLeg).
        // Un Y croissant = "jambes vers le haut" ; tout a (0,0,0) = "membres au centre".
        if (anim) {
            static const char* kBones[] = { "Hips", "Spine", "Head", "LeftUpLeg", "LeftLeg",
                                            "LeftFoot", "LeftArm", "LeftForeArm", "LeftHand" };
            for (const char* n : kBones) {
                const glm::vec3 p = anim->getBoneWorldPosition(n);
                LOG_INFO("[ModelLoader]   %-12s world=(%.3f, %.3f, %.3f)", n, p.x, p.y, p.z);
            }
        }
        // Arbre des noeuds (structure RrTt) : montre l'ordre des wrappers
        // "_$AssimpFbx$_Translation" / "_PreRotation" autour de chaque bone.
        LOG_INFO("[ModelLoader]   Arbre des noeuds :");
        if (model->getRootNode()) {
            dumpNodeTree(model->getRootNode(), 0);
        }
        // Canaux de l'idle (index 0) : montre si l'animation est aussi RrTt
        // (noms "_$AssimpFbx$_Rotation") ou fournit des rotations absolues.
        const aiAnimation* idleAnim = model->getAnimation(0);
        if (idleAnim) {
            LOG_INFO("[ModelLoader]   idle '%s' : %u channels",
                     idleAnim->mName.C_Str(), idleAnim->mNumChannels);
            for (unsigned int i = 0; i < idleAnim->mNumChannels && i < 14; i++) {
                const aiNodeAnim* ch = idleAnim->mChannels[i];
                if (ch) {
                    LOG_INFO("[ModelLoader]     '%s' pos=%u rot=%u scale=%u",
                             ch->mNodeName.C_Str(), ch->mNumPositionKeys,
                             ch->mNumRotationKeys, ch->mNumScalingKeys);
                }
            }
        }
    }
}
