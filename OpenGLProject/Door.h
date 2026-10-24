#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>

#include "constants/door.h"

class Shader;
class Camera;
class Mesh;

// ─────────────────────────────────────────────────────────────────────────────
// Door : porte 3D = rectangle (pave droit largeur x hauteur x epaisseur) qui
// tourne de 90 degres autour de sa charniere (bord gauche, bas = origine du
// repere local de la porte).
//
// Interaction : un contour (outline) pulse autour de la porte des que le
// joueur est a portee (Constants::Door::INTERACT_RANGE) ET regarde dans sa
// direction (cone de regard). toggle(playerPos) ouvre TOUJOURS du cote
// oppose au joueur (poussee : la porte s'eloigne de lui) ou ferme ; update()
// anime la rotation vers l'angle cible.
//
// Collision : la Scene met a jour un collider dynamique via getModelMatrix()
// + getMesh() (l'AABB suit la rotation de la porte).
// ─────────────────────────────────────────────────────────────────────────────
class Door {
public:
    // hingePos : position world de la charniere (bas du bord pivotant).
    // facingYawDeg : orientation de la porte fermee (degres autour de Y).
    // width/height/thickness : dimensions en metres (defaults dans constants/door.h).
    Door(const glm::vec3& hingePos, float facingYawDeg = 0.0f,
         float width = Constants::Door::DEFAULT_WIDTH,
         float height = Constants::Door::DEFAULT_HEIGHT,
         float thickness = Constants::Door::DEFAULT_THICKNESS);
    ~Door() = default;

    // Anime la rotation vers l'angle cible + le contour (fade in/out + pulse).
    void update(float deltaTime);

    // Ouvre/ferme (rotation de 90 degres autour de la charniere).
    // A l'ouverture, le sens de bascule est choisi pour que la porte s'ouvre
    // du cote OPPOSE a playerPos ; le sens reste fige jusqu'a la fermeture.
    void toggle(const glm::vec3& playerPos);

    // Vrai si le joueur est a portee ET regarde dans la direction de la porte.
    // eyePos : position de la caméra ; viewDir : vecteur "devant" de la caméra.
    bool canInteract(const glm::vec3& eyePos, const glm::vec3& viewDir) const;

    // Etat de surlignage (pilote par la Scene chaque frame).
    void setHighlighted(bool highlighted) { m_isHighlighted = highlighted; }
    bool getIsOpen() const { return m_isOpen; }

    // Le battant FERME coupe-t-il le segment [a, b] (plan XZ, meme etage) ?
    // Sert a fropy : il n'ouvre que les portes qui barrent sa trajectoire.
    bool blocksSegment(const glm::vec3& a, const glm::vec3& b) const {
        return panelBlocksSegment(m_hingePos, m_facingYawDeg, m_width, m_height, a, b);
    }
    static bool panelBlocksSegment(const glm::vec3& hinge, float yawDeg, float width, float height,
                                   const glm::vec3& a, const glm::vec3& b);

    // Repere monde : translate(charniere) * rotate(yaw + angle courant).
    glm::mat4 getModelMatrix() const;
    // Centre world de la porte (sert aux tests de distance / regard).
    glm::vec3 getCenter() const;

    Mesh* getMesh() const { return m_mesh.get(); }

    // Dessine d'abord le contour (outlineShader = shader "outline", coque
    // elargie autour du centre, dessinee AVANT la porte : seul le liseré
    // extérieur reste visible), puis la porte (matiere unie).
    void draw(Shader* shader, Shader* outlineShader, Camera* camera);

private:
    // Construit le pave local : x in [0,width] (charniere en x=0),
    // y in [0,height], z in [-thickness/2, +thickness/2].
    std::unique_ptr<Mesh> buildMesh() const;

    // Opacite du contour final (fade * pulse * OUTLINE_OPACITY), 0 si pas
    // interactive.
    float currentOutlineAlpha() const;

    glm::vec3 m_hingePos;
    float m_facingYawDeg;
    float m_width;
    float m_height;
    float m_thickness;

    bool  m_isOpen = false;
    bool  m_isHighlighted = false;
    float m_angle = 0.0f;       // angle courant de rotation (degres)
    float m_openSign = 1.0f;    // sens d'ouverture (+1/-1, choisi au toggle)
    float m_highlightFade = 0.0f; // fondu du surlignage (0..1), anime dans update()
    float m_time = 0.0f;        // temps cumule (pulse du contour)

    glm::vec3 m_color;
    std::unique_ptr<Mesh> m_mesh;
};
