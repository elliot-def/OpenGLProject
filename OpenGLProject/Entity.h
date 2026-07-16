#pragma once

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/fwd.hpp>

#include "constants.h"

class CollisionManager;
class Renderer;
class Direction;
class Shader;

// Namespace direction : contient des constantes pour les directions de mouvement
namespace EntityRelativeDirection {
    enum {
        NONE,      // Aucune direction
        FORWARD,   // Avancer
        BACKWARD,  // Reculer
        LEFT,      // Aller a gauche
        RIGHT,     // Aller a droite
        UP,        // Monter
        DOWN       // Descendre
    };
}


// Classe Entity : classe de base pour tous les objets de la scene
// Exemple : joueur, cube, camera, etc.
class Entity
{
public:
    // Constructeur
    // renderer : pointeur vers le renderer, utile pour dessiner l'entité
    Entity(Renderer* renderer, CollisionManager* collisionManager);

    // Destructeur virtuel pour que les classes derives liberent correctement leur memoire
    ~Entity();

    // Methode virtuelle : mise a jour de l'entité (mouvement, physique, logique)
    virtual void update();
    void updatePositionFromEnvironment(float deltaTime);

    // Methode virtuelle : rendu de l'entité (envoie les donnees a OpenGL)
    // Correction : la méthode draw doit prendre un Shader* en paramètre pour permettre l'override
    virtual void draw(Shader* shader);

    // Getters
    glm::vec3 getPosition() const { return m_position; }           // Position de l'entité
    Direction* getDirection() const { return m_direction; } // Direction regardee
    glm::vec3 getDirectionVector() const; // Direction regardee
    bool isGravityEnabled() const { return m_useGravity; }

    void setUseGravity(bool enable) { m_useGravity = enable; }
    void setPosition(const glm::vec3& position) {
        m_position = position;
    }

protected:
    Renderer* m_renderer;     // Renderer pour dessiner et gerer le rendu
    Direction* m_direction;   // Orientation de l'entité (yaw/pitch)
    CollisionManager* m_collisionManager;

    glm::vec3 m_position;     // Position dans l'espace 3D
    bool m_useGravity = true;       // Ton nouvel attribut
    glm::vec3 m_frameMovement{ 0.f }; // Accumulateur de mouvement

};
