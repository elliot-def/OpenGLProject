#pragma once

#include <memory>

// Déclarations anticipées
struct GLFWwindow;
class Camera;
class LightManager;
class Renderer;
class TextureManager;
class InputManager;
class ModelEntity;
class FirstPersonArms;
class CharacterAnimationController;

// ---------------------------------------------------------------------------
// ModelLoader
//
// Charge tous les modèles 3D du jeu (backpack, fropy, avant-bras riggés,
// personnage 3P riggé + clips d'animation Mixamo) sur un thread séparé avec
// contexte OpenGL partagé, puis instancie le CharacterAnimationController.
//
// Le loader possède les entités qu'il crée ; Game n'en garde que des vues
// non-propriétaires (voir Game::adoptLoadedEntities()).
// ---------------------------------------------------------------------------
class ModelLoader {
public:
    ModelLoader(Camera* camera, LightManager* lightManager, Renderer* renderer,
                TextureManager* textureManager, InputManager* inputManager);
    ~ModelLoader();

    // Corps de l'ancien Game::loadModelsAsync(). À exécuter sur le thread de
    // chargement. loaderWindow = contexte GL partagé créé par
    // Window::createSharedContext() (nullptr = contexte courant, fallback synchrone).
    void load(GLFWwindow* loaderWindow);

    // Accès aux entités chargées (détenues par le loader, valides après load())
    ModelEntity* getModelEntity() const { return m_modelEntity.get(); }
    ModelEntity* getFropyEntity() const { return m_fropyEntity.get(); }
    ModelEntity* getHumanEntity() const { return m_humanEntity.get(); }
    ModelEntity* getNPCEntity()     const { return m_npcEntity.get(); }
    FirstPersonArms* getFirstPersonArms() const { return m_firstPersonArms.get(); }
    CharacterAnimationController* getCharacterAnim() const { return m_characterAnim.get(); }

private:
    void loadDecorModels();    // backpack + fropy + avant-bras 1P
    void loadHumanCharacter(); // Megan + animations Mixamo + contrôleur 3P
    void loadNPC();            // PNJ (deuxième instance de Megan avec dialog)

    Camera* m_camera;
    LightManager* m_lightManager;
    Renderer* m_renderer;
    TextureManager* m_textureManager;
    InputManager* m_inputManager;

    std::unique_ptr<ModelEntity> m_modelEntity;
    std::unique_ptr<ModelEntity> m_fropyEntity;
    std::unique_ptr<ModelEntity> m_humanEntity;
    std::unique_ptr<ModelEntity> m_npcEntity;
    std::unique_ptr<FirstPersonArms> m_firstPersonArms;
    std::unique_ptr<CharacterAnimationController> m_characterAnim;
};
