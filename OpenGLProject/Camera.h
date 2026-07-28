#pragma once

#include <glm/glm.hpp>                  // Pour les vecteurs et matrices
#include <glm/ext/matrix_transform.hpp> // Pour glm::lookAt et transformations 3D
#include <iostream>

#include "Renderer.h"  // Gestion du rendu OpenGL
#include "Entity.h"    // Classe de base pour tous les objets du jeu
#include "Direction.h" // Classe pour stocker les angles de rotation de la cam�ra

// Classe Camera : gere la vue 3D dans le jeu
class Camera {
public:
    // Constructeur
    // position : position initiale de la cam�ra
    // direction : angles de rotation de la cam�ra (yaw/pitch)
    Camera(glm::vec3 position = glm::vec3(3.0f, 3.0f, 3.0f), Direction* direction = new Direction(-90.0f, 0.0f));

    // Retourne la matrice "View" (vue de la cam�ra) calcul�e avec glm::lookAt
    // Utilis�e par OpenGL pour savoir d'o� et vers quoi regarder
    // MUST stay const : appelee via Shader::getCamera() qui retourne const Camera*.
    // Si un futur refactor ajoute une ecriture d'etat, Cube::draw() ligne 193 explosera en C2662.
    glm::mat4 getViewMatrix() const;

    // Position actuelle de la cam�ra
	inline glm::vec3 getPosition() const { return m_position; } 
	inline glm::vec3 getFront() const { return m_front; } 
	inline glm::vec3 getUp() const { return m_upVector; } 

    // Met � jour la position et la direction de la cam�ra en suivant une entit�
    // entity : objet � suivre (ex : le joueur)
    void update(Entity* entity);

private:
    Renderer* m_renderer;     // Pointeur vers le renderer pour acc�der au rendu (non utilis� ici)
    Direction* m_direction;   // Contient yaw et pitch pour orienter la cam�ra

    glm::vec3 m_position = glm::vec3(3.0f, 3.0f, 3.0f); // Position actuelle de la cam�ra
    glm::vec3 m_front = glm::vec3(1.0f, 1.0f, 0.0f);    // Direction dans laquelle la cam�ra regarde
    glm::vec3 m_upVector = glm::vec3(0.0f, 1.0f, 0.0f); // Vecteur "haut" de la cam�ra, pour l'orientation
};