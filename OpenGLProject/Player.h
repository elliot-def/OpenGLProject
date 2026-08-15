#pragma once

#include "Entity.h"

#include "constants/player.h"

#include <glm/glm.hpp>

class CollisionManager;
class Camera;

// Classe Player : représente le joueur dans le jeu
// Hérite de Entity et gère le déplacement, la vue et le rendu
class Player : public Entity {
public:
    // Constructeur : prend un pointeur vers le moteur de rendu
    // Appelle le constructeur de la classe de base Entity
    Player(CollisionManager* collisionManager, Renderer* renderer) : Entity(renderer, collisionManager) {
		setUseGravity(true); // Le joueur est affecté par la gravité
    };

    // Destructeur par défaut, aucune ressource supplémentaire à libérer
    ~Player() = default;

    // Met à jour la logique du joueur chaque frame
    // Déplacement, animations, actions
    void update() override;

    // Dessine le joueur à l'écran
    void draw(Shader* shader) override;

    // Traite les touches de direction
    // direction : valeur parmi direction::FORWARD, BACKWARD, LEFT, RIGHT, UP, DOWN
    // speedFactor : facteur analogique (0..1) multipliant la vitesse de
    // deplacement (1.0 = pleine vitesse). Utilise par le stick gauche de la
    // manette, ignore par le clavier (valeur par defaut 1.0).
    void processDirectionKey(int direction, float speedFactor = 1.0f);

    // Déclenche un saut : délègue au CollisionManager qui applique l'impulsion
    // verticale si le joueur est au sol.
    void processJump();

    // Traite les mouvements de la souris
    // yaw : rotation horizontale
    // pitch : rotation verticale
    void processMouseMovements(double yaw, double pitch);

    inline void setCamera(Camera* camera) { m_camera = camera; }

    inline void processFlashLightKey() { m_isFlashlightEnabled = !m_isFlashlightEnabled; };

    void processThirdPersonKey();
    bool isThirdPerson() const;;

    inline void setIsSprinting(bool isSprinting) { m_isSprinting = isSprinting; };

    // Sprint effectif : vrai uniquement si la touche est tenue (m_isSprinting)
    // ET que le joueur est au sol (grounded) ou en no-clip (gravité désactivée).
    // En l'air, la vitesse ET les animations de course retombent sur la marche.
    bool canSprint() const;
    bool getIsSprinting() const;

    // Getters

    inline bool getFlashlightIsEnabled() { return m_isFlashlightEnabled; };
    inline bool getWantsToMove() { return m_wantsToMove; };

    // Facteur de vitesse post-atterrissage (0..1). 1.0 = normal.
    void setPostLandSpeedFactor(float factor) { m_postLandSpeedFactor = factor; }
    float getPostLandSpeedFactor() const { return m_postLandSpeedFactor; };

    // Retourne la position des yeux du joueur (pour la caméra)

    glm::vec3 getEyePosition() const { return m_position + Constants::Player::PLAYER_EYE_HEIGHT; }
private:
    Camera* m_camera = nullptr;
    bool m_isFlashlightEnabled = false;
    bool m_isSprinting = false;
	bool m_wantsToMove = false;
    float m_postLandSpeedFactor = 1.0f;  // facteur de vitesse post-atterrissage
};
