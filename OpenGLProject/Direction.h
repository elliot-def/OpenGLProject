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

    // Retourne le vecteur unitaire representant la direction actuelle.
    // Calcule (cos/sin) UNIQUEMENT si yaw/pitch ont change (flag dirty) : les
    // appels repetes dans la meme frame (Player, Camera, Spotlight...) ne
    // refont plus de trigonometrie.
    glm::vec3 getDirectionVector() const;

    // Retourne un vecteur dirige a 90 degres a droite tout en gardant l'axe Y constant
    // Utile pour calculer des deplacements lateraux
    glm::vec3 rotateRight90KeepY() const {
        glm::vec3 v = getDirectionVector();
        return glm::normalize(glm::vec3(-v.z, 0, v.x));
    }

    glm::vec3 getDirectionVectorKeepY() const {
        glm::vec3 v = getDirectionVector();
        return glm::normalize(glm::vec3(v.x, 0, v.z));
    }

    double getYaw() const { return m_yaw; }
    double getPitch() const { return m_pitch; }
    void setYawPitch(double yaw, double pitch);

    // Version : incrementee a chaque mutation (addDelta/setYawPitch). Sert de
    // signal "dirty" aux caches externes (ex: ModelEntity::getModelMatrix).
    unsigned int getVersion() const { return m_version; }

private:
    double m_yaw;   // Rotation horizontale (gauche-droite)
    double m_pitch; // Rotation verticale (haut-bas)

    // Cache du vecteur de direction : recalcule seulement quand m_dirty est vrai.
    mutable glm::vec3 m_cachedDirection{ 0.0f, 0.0f, 0.0f };
    mutable bool m_dirty = true;
    unsigned int m_version = 0;
};
