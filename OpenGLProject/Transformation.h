#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

/**
 * @class Transformation
 * @brief Classe pour g�rer les transformations 3D (translation, rotation, mise � l'�chelle).
 *
 * Cette classe encapsule une matrice 4x4 et fournit des m�thodes pour appliquer
 * des transformations cumulatives de mani�re fluide.
 */
class Transformation {
public:
    /**
     * @brief Constructeur par d�faut.
     *
     * Initialise la matrice de transformation comme matrice identit�.
     */
    Transformation() : m_trans(glm::mat4(1.0f)) {}

    /**
     * @brief Applique une rotation autour d'un axe donn�.
     * @param axes Vecteur indiquant l'axe de rotation (x, y, z).
     * @param degrees Angle de rotation en degr�s.
     * @return R�f�rence � l'objet pour cha�nage de m�thodes.
     */
    Transformation& rotate(glm::vec3 axes, float degrees) {
        m_trans = glm::rotate(m_trans, glm::radians(degrees), axes);
        return *this;
    }

    /**
     * @brief Applique une mise � l'�chelle.
     * @param scale Vecteur de mise � l'�chelle pour chaque axe.
     * @return R�f�rence � l'objet pour cha�nage de m�thodes.
     */
    Transformation& scale(glm::vec3 scale) {
        m_trans = glm::scale(m_trans, scale);
        return *this;
    }

    /**
     * @brief Applique une translation.
     * @param translation Vecteur de d�placement.
     * @return R�f�rence � l'objet pour cha�nage de m�thodes.
     */
    Transformation& translate(glm::vec3 translation) {
        m_trans = glm::translate(m_trans, translation);
        return *this;
    }

    /**
     * @brief Remplace la matrice de transformation actuelle.
     * @param matrix Nouvelle matrice 4x4 glm::mat4.
     * @return R�f�rence � l'objet pour cha�nage de m�thodes.
     */
    Transformation& setMatrix(const glm::mat4& matrix) {
        m_trans = matrix;
        return *this;
    }

    /**
     * @brief R�cup�re la matrice de transformation actuelle.
     * @return Matrice 4x4 glm::mat4.
     */
    glm::mat4 getMatrix() const { return m_trans; }

private:
    glm::mat4 m_trans; ///< Matrice de transformation cumul�e
};
