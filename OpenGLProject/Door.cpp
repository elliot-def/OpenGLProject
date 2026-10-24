#include "Door.h"

#include "Shader.h"
#include "Camera.h"
#include "Mesh.h"
#include "Vertex.h"
#include "constants/door.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

Door::Door(const glm::vec3& hingePos, float facingYawDeg,
           float width, float height, float thickness)
    : m_hingePos(hingePos), m_facingYawDeg(facingYawDeg),
      m_width(width), m_height(height), m_thickness(thickness),
      m_color(Constants::Door::DOOR_COLOR) {
    m_mesh = buildMesh();
}

// ── Construction du pave local ───────────────────────────────────────────────
// x in [0,width] (charniere en x=0), y in [0,height],
// z in [-thickness/2, +thickness/2]. 24 sommets (normales par face).
std::unique_ptr<Mesh> Door::buildMesh() const {
    const float w = m_width;
    const float h = m_height;
    const float t = m_thickness * 0.5f;

    std::vector<Vertex> vertices = {
        // Face avant (+Z)
        Vertex(0.0f, 0.0f,  t,  0.0f, 0.0f,  1.0f, 0.0f, 0.0f),
        Vertex(w,    0.0f,  t,  0.0f, 0.0f,  1.0f, 1.0f, 0.0f),
        Vertex(w,    h,     t,  0.0f, 0.0f,  1.0f, 1.0f, 1.0f),
        Vertex(0.0f, h,     t,  0.0f, 0.0f,  1.0f, 0.0f, 1.0f),
        // Face arriere (-Z)
        Vertex(w,    0.0f, -t,  0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
        Vertex(0.0f, 0.0f, -t,  0.0f, 0.0f, -1.0f, 1.0f, 0.0f),
        Vertex(0.0f, h,    -t,  0.0f, 0.0f, -1.0f, 1.0f, 1.0f),
        Vertex(w,    h,    -t,  0.0f, 0.0f, -1.0f, 0.0f, 1.0f),
        // Face droite (+X)
        Vertex(w, 0.0f,  t,  1.0f, 0.0f, 0.0f, 0.0f, 0.0f),
        Vertex(w, 0.0f, -t,  1.0f, 0.0f, 0.0f, 1.0f, 0.0f),
        Vertex(w, h,    -t,  1.0f, 0.0f, 0.0f, 1.0f, 1.0f),
        Vertex(w, h,     t,  1.0f, 0.0f, 0.0f, 0.0f, 1.0f),
        // Face gauche (-X)
        Vertex(0.0f, 0.0f, -t, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f),
        Vertex(0.0f, 0.0f,  t, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f),
        Vertex(0.0f, h,     t, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f),
        Vertex(0.0f, h,    -t, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f),
        // Face haute (+Y)
        Vertex(0.0f, h,  t,  0.0f, 1.0f, 0.0f, 0.0f, 0.0f),
        Vertex(w,    h,  t,  0.0f, 1.0f, 0.0f, 1.0f, 0.0f),
        Vertex(w,    h, -t,  0.0f, 1.0f, 0.0f, 1.0f, 1.0f),
        Vertex(0.0f, h, -t,  0.0f, 1.0f, 0.0f, 0.0f, 1.0f),
        // Face basse (-Y)
        Vertex(0.0f, 0.0f, -t,  0.0f, -1.0f, 0.0f, 0.0f, 0.0f),
        Vertex(w,    0.0f, -t,  0.0f, -1.0f, 0.0f, 1.0f, 0.0f),
        Vertex(w,    0.0f,  t,  0.0f, -1.0f, 0.0f, 1.0f, 1.0f),
        Vertex(0.0f, 0.0f,  t,  0.0f, -1.0f, 0.0f, 0.0f, 1.0f),
    };

    std::vector<unsigned int> indices;
    indices.reserve(36);
    for (unsigned int face = 0; face < 6; ++face) {
        unsigned int base = face * 4;
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
        indices.push_back(base + 0);
    }

    return std::make_unique<Mesh>(vertices, indices,
        (unsigned int)VertexAttribute::POSITION |
        (unsigned int)VertexAttribute::NORMAL |
        (unsigned int)VertexAttribute::TEXCOORD);
}

// ── Animation ────────────────────────────────────────────────────────────────

void Door::update(float deltaTime) {
    m_time += deltaTime;

    // Rotation vers l'angle cible (ouverture/fermeture 90 degres) ; m_openSign
    // choisit le sens de bascule (cote oppose au joueur, voir toggle()).
    const float target = m_isOpen ? m_openSign * Constants::Door::OPEN_ANGLE : 0.0f;
    const float step = Constants::Door::OPEN_SPEED * deltaTime;
    if (m_angle < target) {
        m_angle = std::min(target, m_angle + step);
    } else if (m_angle > target) {
        m_angle = std::max(target, m_angle - step);
    }

    // Outline : fade in/out quand la porte devient (ou cesse d'etre) interactive.
    const float fadeTarget = m_isHighlighted ? 1.0f : 0.0f;
    m_highlightFade += (fadeTarget - m_highlightFade) *
                       std::min(1.0f, deltaTime * Constants::Door::OUTLINE_FADE_SPEED);
}

// Opacite du contour : 0 si pas interactive, sinon fade * onde entre ~0.6 et 1,
// attenuee par OUTLINE_OPACITY (intensite globale du contour).
float Door::currentOutlineAlpha() const {
    const float pulse = 0.6f + 0.4f * std::sin(m_time * Constants::Door::OUTLINE_PULSE_SPEED);
    return m_highlightFade * pulse * Constants::Door::OUTLINE_OPACITY;
}

// Ouvre ou ferme la porte. A l'ouverture, le sens est choisi pour que le
// battant bascule du cote OPPOSE au joueur (poussee).
// Convention : +OPEN_ANGLE fait tourner le battant vers le local -Z (le point
// local (w,0,0) devient (w·cos, 0, -w·sin)), donc on prend le signe de la
// composante du joueur sur la normale locale +Z de la porte fermee.
void Door::toggle(const glm::vec3& playerPos) {
    if (m_isOpen) {
        m_isOpen = false;   // fermeture : retour a 0, sens courant conserve
        return;
    }
    // Normale locale +Z de la porte fermee, en monde : R_y(yaw) * (0,0,1).
    const float yaw = glm::radians(m_facingYawDeg);
    const glm::vec3 normalZ(std::sin(yaw), 0.0f, std::cos(yaw));
    const float side = glm::dot(playerPos - m_hingePos, normalZ);
    if (std::abs(side) > 1e-3f) {   // joueur pile dans le plan : sens precedent
        m_openSign = side > 0.0f ? 1.0f : -1.0f;
    }
    m_isOpen = true;
}

// ── Interaction ──────────────────────────────────────────────────────────────

bool Door::panelBlocksSegment(const glm::vec3& hinge, float yawDeg, float width, float height,
                              const glm::vec3& a, const glm::vec3& b) {
    // Etage : le segment doit passer a hauteur du battant (pieds entre le bas et le haut).
    if (a.y < hinge.y - 1.0f || a.y > hinge.y + height - 0.5f) return false;
    // Battant ferme : de la charniere a charniere + width * R_y(yaw) * (1,0,0) = (cos, 0, -sin).
    const float yaw = glm::radians(yawDeg);
    const glm::vec2 p(hinge.x, hinge.z), r(width * std::cos(yaw), -width * std::sin(yaw));
    const glm::vec2 q(a.x, a.z), s(b.x - a.x, b.z - a.z);
    const float den = r.x * s.y - r.y * s.x;
    if (std::abs(den) < 1e-6f) return false;   // paralleles
    const glm::vec2 qp = q - p;
    const float t = (qp.x * s.y - qp.y * s.x) / den;   // position sur le battant
    const float u = (qp.x * r.y - qp.y * r.x) / den;   // position sur le segment
    return t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f;
}

bool Door::canInteract(const glm::vec3& eyePos, const glm::vec3& viewDir) const {
    const glm::vec3 toDoor = getCenter() - eyePos;
    const float dist = glm::length(toDoor);
    if (dist > Constants::Door::INTERACT_RANGE || dist < 1e-4f) {
        return false;
    }
    const float viewLen = glm::length(viewDir);
    if (viewLen < 1e-4f) {
        return false;
    }
    const glm::vec3 dir = toDoor / dist;
    return glm::dot(dir, viewDir / viewLen) >= Constants::Door::INTERACT_LOOK_DOT;
}

// ── Transformations ──────────────────────────────────────────────────────────

glm::mat4 Door::getModelMatrix() const {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), m_hingePos);
    // La porte fermee fait face a facingYawDeg ; l'ouverture pivote autour de
    // la charniere (axe Y) de OPEN_ANGLE degres.
    model = glm::rotate(model, glm::radians(m_facingYawDeg + m_angle),
                        glm::vec3(0.0f, 1.0f, 0.0f));
    return model;
}

glm::vec3 Door::getCenter() const {
    // Centre local (width/2, height/2, 0) transforme par le modele courant.
    const glm::vec4 local(m_width * 0.5f, m_height * 0.5f, 0.0f, 1.0f);
    return glm::vec3(getModelMatrix() * local);
}

// ── Rendu ────────────────────────────────────────────────────────────────────

void Door::draw(Shader* shader, Shader* outlineShader, Camera* camera) {
    const float outlineAlpha = currentOutlineAlpha();

    // ── OUTLINE PASS ────────────────────────────────────────────────────────────────────────────
    // Coque legerement elargie autour du centre de la porte, couleur plate,
    // ecriture de profondeur off + alpha blending. Dessinee AVANT la porte :
    // la porte (opaque) ecrit la profondeur et recouvre l'interieur de la
    // coque -> seul le liseré extérieur reste visible (contour net, pas de
    // glow sur la matiere).
    if (outlineShader && outlineAlpha > 0.01f) {
        const glm::vec3 center = getCenter();
        glm::mat4 outlineModel = glm::translate(glm::mat4(1.0f), center);
        outlineModel = glm::scale(outlineModel,
                                  glm::vec3(1.0f + Constants::Door::OUTLINE_THICKNESS));
        outlineModel = glm::translate(outlineModel, -center);
        outlineModel = outlineModel * getModelMatrix();

        outlineShader->use();
        outlineShader->setMat4("uModel", outlineModel);
        outlineShader->setMat4("uView", camera->getViewMatrix());
        outlineShader->setMat4("uProjection", shader->getProjection());
        outlineShader->setVec3("uOutlineColor", Constants::Door::OUTLINE_COLOR);
        outlineShader->setFloat("uOutlineAlpha", outlineAlpha);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        m_mesh->draw();
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    shader->use();
    shader->setMat4("model", getModelMatrix());
    shader->setVec3("uColor", m_color);
    m_mesh->draw();
}
