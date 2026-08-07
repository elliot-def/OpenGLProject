#include "win_compat.h"

#include "ModelLoader.h"
#include "ModelEntity.h"
#include "FirstPersonArms.h"
#include "CharacterAnimationController.h"
#include "InputManager.h"
#include "Animator.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <cstdio>
#include <string>
#include <vector>

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
    printf("[ModelLoader]   → Chargement du backpack...\n");
    m_modelEntity = std::make_unique<ModelEntity>(m_camera, m_lightManager, m_renderer,
                                                  "./res/models/backpack/backpack.obj", m_textureManager);

    printf("[ModelLoader]   → Chargement de fropy (low poly)...\n");
    m_fropyEntity = std::make_unique<ModelEntity>(m_camera, m_lightManager, m_renderer,
                                                  "./res/models/fropy/fropy_low_poly.obj", m_textureManager);
    m_fropyEntity->setPosition(glm::vec3(3.0f, 5.0f, 0.0f));
    m_fropyEntity->setSpin(20.0f, glm::vec3(0.0f, 1.0f, 0.0f));

    printf("[ModelLoader]   → Chargement des bras (rigges)...\n");
    m_firstPersonArms = std::make_unique<FirstPersonArms>(m_camera, m_lightManager,
                                                          "./res/rigging/arm/arms_rig.glb", m_textureManager);
    m_inputManager->setFirstPersonArms(m_firstPersonArms.get());
}

void ModelLoader::loadHumanCharacter() {
    printf("[ModelLoader]   → Chargement de Megan (rigge)...\n");
    m_humanEntity = std::make_unique<ModelEntity>(m_camera, m_lightManager, m_renderer,
                                                  "./res/rigging/mixamo/models/Megan.fbx", m_textureManager);

    Model* model = m_humanEntity->getModel();
    const aiScene* scene = model->getScene();

    // ── Debug chargement : un echec ici = aucun personnage en 3P ────────
    if (!scene || model->getMeshes().empty()) {
        printf("[ModelLoader]   !! ERREUR : le modele n'a pas pu etre charge "
               "(scene=%p, meshes=%zu) -> rien ne sera affiche en 3P.\n",
               (const void*)scene, model->getMeshes().size());
        printf("[ModelLoader]   !! Verifier que le fichier est lisible par Assimp "
               "(FBX 2011/2012/2013 ou GLB/GLTF) : un ancien FBX 6.x est refuse.\n");
    } else {
        printf("[ModelLoader]   Modele OK : %zu meshes, %u bones, %zu animations embarquees, "
               "bbox size=(%.2f, %.2f, %.2f)\n",
               model->getMeshes().size(),
               static_cast<unsigned int>(model->getBoneInfoMap().size()),
               model->getNumAnimations(),
               model->getBoundingBox().getSize().x,
               model->getBoundingBox().getSize().y,
               model->getBoundingBox().getSize().z);
    }

    // Auto-scale : hauteur cible ~1.8 unites (~1.80m) divisee par la hauteur
    // reelle de la bounding box du modele (qui inclut le x100 FBX).
    {
        const float modelHeight = model->getBoundingBox().getSize().y;
        constexpr float targetHeight = 1.8f;
        if (modelHeight > 0.001f) {
            m_humanEntity->setScale(targetHeight / modelHeight);
            printf("[ModelLoader]   Megan auto-scale: %.4f (model=%.1f -> target=%.1f)\n",
                   targetHeight / modelHeight, modelHeight, targetHeight);
        } else {
            printf("[ModelLoader]   Pas d'auto-scale (hauteur modele=%.4f, < 0.001)\n", modelHeight);
        }
    }

    // Charger les animations externes (FBX separes Mixamo)
    {
        const std::string animDir = "./res/rigging/mixamo/animation/";
        std::vector<std::string> animPaths = {
            animDir + "idle.fbx",
            animDir + "walking.fbx",
            animDir + "standard run.fbx",
            animDir + "jump.fbx",
            animDir + "left strafe.fbx",
            animDir + "right strafe.fbx",
            animDir + "left strafe walking.fbx",
            animDir + "right strafe walking.fbx",
            animDir + "left turn 90.fbx",
            animDir + "right turn 90.fbx",
            animDir + "Running Jump.fbx",
            animDir + "Walking Backwards.fbx",
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

        m_humanEntity->setIdleAnimIndex(static_cast<int>(base + 0));       // idle.fbx
        m_humanEntity->setWalkAnimIndex(static_cast<int>(base + 1));       // walking.fbx
        m_humanEntity->setRunAnimIndex(static_cast<int>(base + 2));        // standard run.fbx
        m_humanEntity->setJumpIdx(static_cast<int>(base + 3));             // jump.fbx
        m_humanEntity->setStrafeLeftIdx(static_cast<int>(base + 4));       // left strafe.fbx
        m_humanEntity->setStrafeRightIdx(static_cast<int>(base + 5));      // right strafe.fbx
        m_humanEntity->setStrafeWalkLeftIdx(static_cast<int>(base + 6));   // left strafe walking.fbx
        m_humanEntity->setStrafeWalkRightIdx(static_cast<int>(base + 7));  // right strafe walking.fbx
        m_humanEntity->setTurnLeftIdx(static_cast<int>(base + 8));         // left turn 90.fbx
        m_humanEntity->setTurnRightIdx(static_cast<int>(base + 9));        // right turn 90.fbx
        m_humanEntity->setRunJumpIdx(static_cast<int>(base + 10));         // Running Jump.fbx
        m_humanEntity->setWalkBackIdx(static_cast<int>(base + 11));        // Walking Backwards.fbx
        // punch = -1 (pas d'anim de punch)
        // rest  = -1 (pas d'anim de rest)

        // ── Debug : liste complete + validation du mapping ──────────────
        printf("[ModelLoader]   %zu animations au total (%zu embarquees + %zu externes) :\n",
               totalAnims, base, model->getNumExternalAnimations());
        for (size_t i = 0; i < totalAnims; i++) {
            const aiAnimation* anim = model->getAnimation(i);
            printf("[ModelLoader]     [%2zu] \"%s\"%s\n", i,
                   anim ? anim->mName.C_Str() : "(null)",
                   i < base ? "  (embarquee)" : "  (externe)");
        }
        printf("[ModelLoader]   Mapping : idle=%d walk=%d run=%d jump=%d strafeL=%d strafeR=%d "
               "strafeWL=%d strafeWR=%d turnL=%d turnR=%d runJump=%d walkBack=%d\n",
               m_humanEntity->getIdleAnimIndex(), m_humanEntity->getWalkAnimIndex(),
               m_humanEntity->getRunAnimIndex(), m_humanEntity->getJumpIdx(),
               m_humanEntity->getStrafeLeftIdx(), m_humanEntity->getStrafeRightIdx(),
               m_humanEntity->getStrafeWalkLeftIdx(), m_humanEntity->getStrafeWalkRightIdx(),
               m_humanEntity->getTurnLeftIdx(), m_humanEntity->getTurnRightIdx(),
               m_humanEntity->getRunJumpIdx(), m_humanEntity->getWalkBackIdx());
        if (base + 11 >= totalAnims) {
            printf("[ModelLoader]   !! ATTENTION : dernier clip externe (index %zu) >= nombre "
                   "d'animations (%zu) -> tous les clips n'existent pas.\n", base + 11, totalAnims);
        }

        // Jouer l'idle
        const int idleIdx = m_humanEntity->getIdleAnimIndex();
        if (idleIdx >= 0 && idleIdx < static_cast<int>(totalAnims)) {
            m_humanEntity->getAnimator()->playAnimation(static_cast<unsigned int>(idleIdx), true);
            m_humanEntity->getAnimator()->update(0.0f);
        }
    }

    // Controleur d'animation du personnage 3P (extrait de Game::update)
    m_characterAnim = std::make_unique<CharacterAnimationController>(m_humanEntity.get(), m_inputManager);
}
