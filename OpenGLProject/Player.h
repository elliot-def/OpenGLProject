#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include "Entity.h"

class CollisionManager;

// Classe Player : représente le joueur dans le jeu
// Hérite de Entity et gère le déplacement, la vue et le rendu
class Player : public Entity {
public:
    // Constructeur : prend un pointeur vers le moteur de rendu
    // Appelle le constructeur de la classe de base Entity
    Player(CollisionManager* collisionManager, Renderer* renderer) : Entity(renderer), m_collisionManager(collisionManager) {};

    // Destructeur par défaut, aucune ressource supplémentaire à libérer
    ~Player() = default;

    // Met à jour la logique du joueur chaque frame
    // Déplacement, animations, actions
    void update() override;

    // Dessine le joueur à l'écran
    void draw(Shader* shader) override;

    // Traite les touches de direction
    // direction : valeur parmi direction::FORWARD, BACKWARD, LEFT, RIGHT, UP, DOWN
    void processDirectionKey(int direction);

    // Traite les mouvements de la souris
    // yaw : rotation horizontale
    // pitch : rotation verticale
    void processMouseMovements(double yaw, double pitch);

    inline void processFlashLightKey() { m_isFlashlightEnabled = !m_isFlashlightEnabled; };

    inline bool getFlashlightIsEnabled() { return m_isFlashlightEnabled; };

    inline void setIsSprinting(bool isSprinting) { m_isSprinting = isSprinting; };
private:
    CollisionManager* m_collisionManager;
    bool m_isFlashlightEnabled = false;
    bool m_isSprinting = false;
};
