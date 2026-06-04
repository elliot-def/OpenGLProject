#pragma once

#include <glm/glm.hpp>                  // Pour les vecteurs et matrices
#include <glm/ext/matrix_transform.hpp> // Pour glm::lookAt et transformations 3D
#include <iostream>

#include "Renderer.h"  // Gestion du rendu OpenGL
#include "Entity.h"    // Classe de base pour tous les objets du jeu
#include "Direction.h" // Classe pour stocker les angles de rotation de la caméra

// Classe Camera : gere la vue 3D dans le jeu
class Camera {
public:
    // Constructeur
    // position : position initiale de la caméra
    // direction : angles de rotation de la caméra (yaw/pitch)
    Camera(glm::vec3 position = glm::vec3(3.0f, 3.0f, 3.0f), Direction* direction = new Direction(-90.0f, 0.0f));

    // Retourne la matrice "View" (vue de la caméra) calculée avec glm::lookAt
    // Utilisée par OpenGL pour savoir d'où et vers quoi regarder
    glm::mat4 getViewMatrix();

    // Position actuelle de la caméra
	inline glm::vec3 getPosition() const { return m_position; } 
	inline glm::vec3 getFront() const { return m_front; } 
	inline glm::vec3 getUp() const { return m_upVector; } 

    // Met à jour la position et la direction de la caméra en suivant une entité
    // entity : objet à suivre (ex : le joueur)
    void update(Entity* entity);

private:
    Renderer* m_renderer;     // Pointeur vers le renderer pour accéder au rendu (non utilisé ici)
    Direction* m_direction;   // Contient yaw et pitch pour orienter la caméra

    glm::vec3 m_position = glm::vec3(3.0f, 3.0f, 3.0f); // Position actuelle de la caméra
    glm::vec3 m_front = glm::vec3(1.0f, 1.0f, 0.0f);    // Direction dans laquelle la caméra regarde
    glm::vec3 m_upVector = glm::vec3(0.0f, 1.0f, 0.0f); // Vecteur "haut" de la caméra, pour l'orientation
};