#pragma once
#include <glm/glm.hpp>

// Classe Direction : represente l'orientation d'un objet ou d'une camera
class Direction
{
public:
    // Constructeur
    // yaw : rotation autour de l'axe Y (gauche-droite)
    // pitch : rotation autour de l'axe X (haut-bas)
    Direction(double yaw = 0.0, double pitch = 0.0);

    // Ajoute un delta a la rotation (ex : mouvement de souris)
    // deltaX : changement horizontal (yaw)
    // deltaY : changement vertical (pitch)
    void addDelta(double deltaX, double deltaY);

    // Retourne le vecteur unitaire representant la direction actuelle
    glm::vec3 getDirectionVector();

    // Retourne un vecteur dirige a 90 degres a droite tout en gardant l'axe Y constant
    // Utile pour calculer des deplacements lateraux
    glm::vec3 rotateRight90KeepY() {
        glm::vec3 v = getDirectionVector();
        return glm::normalize(glm::vec3(-v.z, 0, v.x));
    }

    glm::vec3 getDirectionVectorKeepY() {
        glm::vec3 v = getDirectionVector();
        return glm::normalize(glm::vec3(v.x, 0, v.z));
    }

private:
    double m_yaw;   // Rotation horizontale (gauche-droite)
    double m_pitch; // Rotation verticale (haut-bas)
};
