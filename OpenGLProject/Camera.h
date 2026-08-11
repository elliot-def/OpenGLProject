#pragma once

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include "Direction.h"

class Entity;
class Renderer;
class CollisionManager;

class Camera {
public:
    // Constructeur
    // position : position initiale de la caméra
    // direction : angles de rotation de la caméra (yaw/pitch)
    Camera(glm::vec3 position = glm::vec3(3.0f, 3.0f, 3.0f), Direction* direction = new Direction(-90.0f, 0.0f));

    // Retourne la matrice "View" (vue de la caméra) calculée avec glm::lookAt
    // Utilisée par OpenGL pour savoir d'où et vers quoi regarder
    // MUST stay const : appelee via Shader::getCamera() qui retourne const Camera*.
    // Si un futur refactor ajoute une ecriture d'etat, Cube::draw() ligne 193 explosera en C2662.
    glm::mat4 getViewMatrix() const;

    // Position actuelle de la caméra
	inline glm::vec3 getPosition() const { return m_position; } 
	inline glm::vec3 getFront() const { return m_front; } 
	inline glm::vec3 getUp() const { return m_upVector; } 

    // Met à jour la position et la direction de la caméra en suivant une entité
    // entity : objet à suivre (ex : le joueur)
    void update(Entity* entity);

    // Bascule entre vue première personne et troisième personne
    void toggleCameraMode() { m_isThirdPerson = !m_isThirdPerson; }
    bool isThirdPerson() const { return m_isThirdPerson; }

    void setCollisionManager(CollisionManager* cm) { m_collisionManager = cm; }
    void setRenderer(Renderer* r) { m_renderer = r; }

    // Positionne la camera a une position exacte (utilise pour suivre les
    // epaules du modele anime en 1P).
    void setPosition(const glm::vec3& pos) { m_position = pos; }

    // Offset de mouvement lisse (direction de marche × lerp). Necessaire dans
    // Game::update() pour le reappliquer apres le tracking de tete.
    glm::vec3 getSmoothedMovementOffset() const { return m_smoothedMovementOffset; }

private:
    Renderer* m_renderer;     // Pointeur vers le renderer pour accéder au rendu (non utilisé ici)
    Direction* m_direction;   // Contient yaw et pitch pour orienter la caméra

    glm::vec3 m_position = glm::vec3(3.0f, 3.0f, 3.0f); // Position actuelle de la caméra
    glm::vec3 m_front = glm::vec3(1.0f, 1.0f, 0.0f);    // Direction dans laquelle la caméra regarde
    glm::vec3 m_upVector = glm::vec3(0.0f, 1.0f, 0.0f); // Vecteur "haut" de la caméra, pour l'orientation

    CollisionManager* m_collisionManager = nullptr;

    bool m_isThirdPerson = false; // false = première personne, true = troisième personne
    bool m_wasFirstPerson = true; // Pour snap au moment du toggle

    glm::vec3 m_smoothedMovementOffset{ 0.f }; // Offset de mouvement lisse (lerp) pour la 1P
};