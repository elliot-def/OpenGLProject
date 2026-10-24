#include "SkySystem.h"

#include <algorithm>
#include <cmath>

#include "ModelEntity.h"
#include "Camera.h"
#include "LightManager.h"
#include "CollisionManager.h"
#include "ShaderManager.h"
#include "Shader.h"
#include "MapLoader.h"
#include "Direction.h"
#include "Log.h"

#include "constants/resource.h"

#include <glad/glad.h>

SkySystem::SkySystem(Camera* camera, LightManager* lightManager, Renderer* renderer,
                     TextureManager* textureManager, ShaderManager* shaderManager,
                     CollisionManager* collisionManager)
    : m_camera(camera), m_lightManager(lightManager), m_renderer(renderer),
      m_textureManager(textureManager), m_collisionManager(collisionManager) {
    m_glareShader = shaderManager->getShader("glare");
    glGenVertexArrays(1, &m_glareVao);
}

SkySystem::~SkySystem() {
    if (m_glareVao) glDeleteVertexArrays(1, &m_glareVao);
}

// Tete placee a l'oppose du rayonnement du soleil (le dir light pointe vers la scene),
// a sunHead.distance du centre du village ; couleur et spot selon la carte.
void SkySystem::load(const MapSun& sun, const MapSunHead& head) {
    glm::vec3 sunDir = sun.direction;
    if (glm::length(sunDir) < 1e-4f) sunDir = glm::vec3(0.0f, -1.0f, 0.0f);
    sunDir = glm::normalize(sunDir);
    const glm::vec3 mapCenter(0.0f, 0.0f, -10.0f);          // centre du village
    m_head = std::make_unique<ModelEntity>(m_camera, m_lightManager, m_renderer,
                                              Constants::Resource::MODEL_FROPY_HEAD,
                                              m_textureManager);
    const MapSunHead& sunCfg = head;
    m_color = sunCfg.color;
    m_lightManager->setSunSpotColor(sunCfg.color * sunCfg.spotIntensity);
    m_head->setScale(sunCfg.scale);                       // tête lointaine (haut dans le ciel)
    m_head->setPosition(mapCenter - sunDir * sunCfg.distance);
    // Face (local +Z) tournée vers la scène : yaw = atan2(fz, fx) en degrés.
    m_head->setDirection(Direction(glm::degrees(std::atan2(-sunDir.z, -sunDir.x)), 0.0));
    LOG_INFO("[Map] Tete du ciel (soleil/lune) a (%.1f, %.1f, %.1f), echelle %.1f",
             (mapCenter - sunDir * sunCfg.distance).x, (mapCenter - sunDir * sunCfg.distance).y,
             (mapCenter - sunDir * sunCfg.distance).z, sunCfg.scale);

}

void SkySystem::update(float deltaTime, const glm::vec3* gazeTarget) {
    // La tete regarde la cible (la creature) et l'eclaire de la spot du soleil.
    if (m_head && gazeTarget) {
        const glm::vec3 from = m_head->getPosition();
        const glm::vec3 d = glm::normalize(*gazeTarget - from);
        m_head->setDirection(Direction(glm::degrees(std::atan2(d.z, d.x)), 0.0));
        m_head->setPitchDeg(glm::degrees(std::asin(-d.y)));
        m_lightManager->aimSunSpot(from, *gazeTarget);
    }

    // Eblouissement : intensite selon l'angle entre le regard et le soleil, annulee
    // si un obstacle coupe la ligne camera -> soleil ; lissee dans le temps.
    m_glareTime += deltaTime;
    float target = 0.0f, targetDazzle = 0.0f;
    if (m_head) {
        const glm::vec3 cam = m_camera->getPosition();
        const glm::vec3 toSun = m_head->getPosition() - cam;
        const float dist = glm::length(toSun);
        const float facing = dist > 1e-3f ? glm::dot(m_camera->getFront(), toSun / dist) : 0.0f;
        const float wide = glm::smoothstep(std::cos(glm::radians(55.0f)), std::cos(glm::radians(6.0f)), facing);
        if (wide > 0.0f) {
            // Rayon exact camera -> soleil : le moindre obstacle (mur mince, tronc, feuillage,
            // toit, porte fermee) coupe l'eblouissement.
            const bool visible = !m_collisionManager->segmentBlocked(cam, m_head->getPosition());
            if (visible) {
                target = wide;
                targetDazzle = glm::smoothstep(std::cos(glm::radians(22.0f)), std::cos(glm::radians(2.0f)), facing);
            }
        }
    }
    const float k = std::min(1.0f, deltaTime * 6.0f);
    m_glare  += (target - m_glare) * k;
    m_dazzle += (targetDazzle - m_dazzle) * k;
}

void SkySystem::drawGlare(Shader* projectionSource) {
    if (!m_glareShader || !m_head || !projectionSource || !(m_glare >= 0.003f)) return;   // !(>=) : rejette aussi NaN
    const glm::mat4 proj = projectionSource->getProjection();
    const glm::vec4 clip = proj * m_camera->getViewMatrix() * glm::vec4(m_head->getPosition(), 1.0f);
    if (clip.w <= 0.01f) return;   // derriere la camera

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);   // melange "screen" : ne sature jamais au-dela de 1
    m_glareShader->use();
    m_glareShader->setVec2("sunNdc", glm::vec2(clip.x / clip.w, clip.y / clip.w));
    m_glareShader->setFloat("aspect", proj[1][1] / proj[0][0]);
    m_glareShader->setFloat("intensity", m_glare);
    m_glareShader->setFloat("dazzle", m_dazzle);
    // Teinte tiree de la couleur du ciel : soleil jaune -> eblouissement chaud, lune blanc-gris -> blanc.
    glm::vec3 tint = glm::mix(m_color, glm::vec3(1.0f), 0.45f);
    tint /= std::max(tint.x, std::max(tint.y, tint.z));
    m_glareShader->setVec3("glareTint", tint);
    m_glareShader->setFloat("time", m_glareTime);
    glBindVertexArray(m_glareVao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void SkySystem::drawHead(Shader* doorShader) {
    if (!m_head || !doorShader) return;
    doorShader->use();
    doorShader->setVec3("uColor", m_color);
    doorShader->setFloat("uNoFog", 1.0f);   // visible de loin malgre le brouillard
    m_head->draw(doorShader);
    doorShader->setFloat("uNoFog", 0.0f);
}
