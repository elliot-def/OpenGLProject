#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <glm/glm.hpp>

#include "Entity.h"

class CollisionManager;
class Camera;

// Classe Player : repr�sente le joueur dans le jeu
// H�rite de Entity et g�re le d�placement, la vue et le rendu
class Player : public Entity {
public:
    // Constructeur : prend un pointeur vers le moteur de rendu
    // Appelle le constructeur de la classe de base Entity
    Player(CollisionManager* collisionManager, Renderer* renderer) : Entity(renderer, collisionManager) {
		setUseGravity(true); // Le joueur est affect� par la gravit�
    };

    // Destructeur par d�faut, aucune ressource suppl�mentaire � lib�rer
    ~Player() = default;

    // Met � jour la logique du joueur chaque frame
    // D�placement, animations, actions
    void update() override;

    // Dessine le joueur � l'�cran
    void draw(Shader* shader) override;

    // Traite les touches de direction
    // direction : valeur parmi direction::FORWARD, BACKWARD, LEFT, RIGHT, UP, DOWN
    void processDirectionKey(int direction);

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

    // Getters

    inline bool getFlashlightIsEnabled() { return m_isFlashlightEnabled; };
    inline bool getIsSprinting() { return m_isSprinting; };
    inline bool getWantsToMove() { return m_wantsToMove; };

    // Retourne la position des yeux du joueur (pour la cam�ra)

    glm::vec3 getEyePosition() const { return m_position + Constants::PLAYER_EYE_HEIGHT; }
private:
    Camera* m_camera = nullptr;
    bool m_isFlashlightEnabled = false;
    bool m_isSprinting = false;
	bool m_wantsToMove = false;
};
