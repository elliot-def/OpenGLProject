#pragma once

#include <memory>
#include <glm/glm.hpp>

class Camera;
class LightManager;
class Renderer;
class TextureManager;
class ShaderManager;
class CollisionManager;
class ModelEntity;
class Shader;
struct MapSun;
struct MapSunHead;

// ─────────────────────────────────────────────────────────────────────────────
// SkySystem : la tete de fropy dans le ciel (soleil jaune / lune blanc-gris), qui
// regarde une cible (la creature) et l'eclaire de la spot "sunSpot", et
// l'eblouissement plein ecran quand le joueur regarde le soleil sans obstacle.
// ─────────────────────────────────────────────────────────────────────────────
class SkySystem {
public:
    SkySystem(Camera* camera, LightManager* lightManager, Renderer* renderer,
              TextureManager* textureManager, ShaderManager* shaderManager,
              CollisionManager* collisionManager);
    ~SkySystem();

    // Place la tete selon l'ambiance de la carte (direction du soleil + "sunHead").
    void load(const MapSun& sun, const MapSunHead& head);

    // gazeTarget : point regarde/eclaire (nullptr = garde l'orientation).
    void update(float deltaTime, const glm::vec3* gazeTarget);

    // Tete (couleur unie, sans brouillard) avec le shader "door".
    void drawHead(Shader* doorShader);
    // Eblouissement (apres toute la scene). projectionSource : shader dont la
    // projection est utilisee pour placer le soleil a l'ecran.
    void drawGlare(Shader* projectionSource);

private:
    Camera* m_camera;
    LightManager* m_lightManager;
    Renderer* m_renderer;
    TextureManager* m_textureManager;
    CollisionManager* m_collisionManager;

    std::unique_ptr<ModelEntity> m_head;
    glm::vec3 m_color{ 1.0f, 0.92f, 0.25f };   // jaune (soleil) / blanc-gris (lune)

    Shader* m_glareShader = nullptr;   // "glare" : eblouissement plein ecran
    unsigned int m_glareVao = 0;       // VAO vide (quad genere par gl_VertexID)
    float m_glare = 0.0f;              // halo + rayons, lisse 0..1
    float m_dazzle = 0.0f;             // voile d'eblouissement, lisse 0..1
    float m_glareTime = 0.0f;
};
