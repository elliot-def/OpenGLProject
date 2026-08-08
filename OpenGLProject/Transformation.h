#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

/**
 * @class Transformation
 * @brief Classe pour gérer les transformations 3D (translation, rotation, mise à l'échelle).
 *
 * Cette classe encapsule une matrice 4x4 et fournit des méthodes pour appliquer
 * des transformations cumulatives de manière fluide.
 */
class Transformation {
public:
    /**
     * @brief Constructeur par défaut.
     *
     * Initialise la matrice de transformation comme matrice identité.
     */
    Transformation() : m_trans(glm::mat4(1.0f)) {}

    /**
     * @brief Applique une rotation autour d'un axe donné.
     * @param axes Vecteur indiquant l'axe de rotation (x, y, z).
     * @param degrees Angle de rotation en degrés.
     * @return Référence à l'objet pour chaînage de méthodes.
     */
    Transformation& rotate(glm::vec3 axes, float degrees) {
        m_trans = glm::rotate(m_trans, glm::radians(degrees), axes);
        return *this;
    }

    /**
     * @brief Applique une mise à l'échelle.
     * @param scale Vecteur de mise à l'échelle pour chaque axe.
     * @return Référence à l'objet pour chaînage de méthodes.
     */
    Transformation& scale(glm::vec3 scale) {
        m_trans = glm::scale(m_trans, scale);
        return *this;
    }

    /**
     * @brief Applique une translation.
     * @param translation Vecteur de déplacement.
     * @return Référence à l'objet pour chaînage de méthodes.
     */
    Transformation& translate(glm::vec3 translation) {
        m_trans = glm::translate(m_trans, translation);
        return *this;
    }

    /**
     * @brief Rotation autour d'un point pivot (spin sur place).
     * @param center Pivot (ex: centre du cube) en world-space.
     * @param axis Axe de rotation.
     * @param angleDeg Angle courant en degrés.
     * @return Référence à l'objet pour chaînage de méthodes.
     */
    Transformation& spinAroundSelf(const glm::vec3& center, const glm::vec3& axis, float angleDeg) {
        m_trans = glm::translate(glm::mat4(1.0f), center) *
                  glm::rotate(glm::mat4(1.0f), glm::radians(angleDeg), axis) *
                  glm::translate(glm::mat4(1.0f), -center);
        return *this;
    }

    /**
     * @brief Remplace la matrice de transformation actuelle.
     * @param matrix Nouvelle matrice 4x4 glm::mat4.
     * @return Référence à l'objet pour chaînage de méthodes.
     */
    Transformation& setMatrix(const glm::mat4& matrix) {
        m_trans = matrix;
        return *this;
    }

    /**
     * @brief Récupère la matrice de transformation actuelle.
     * @return Matrice 4x4 glm::mat4.
     */
    glm::mat4 getMatrix() const { return m_trans; }

private:
    glm::mat4 m_trans; ///< Matrice de transformation cumulée
};
