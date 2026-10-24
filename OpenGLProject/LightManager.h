#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>

#include "constants/shader.h"

class Shader;
class LightSource;
class Spotlight;
class Renderer;
class Player;

struct DirLight {
    glm::vec3 direction;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
};

class LightManager {
public:
    LightManager(Renderer* renderer, Player* player);
    ~LightManager();

    // Retourne l'index de la lumière (attendu par setLightRoom).
    size_t addPointLight(LightSource* light);
    // Rechargement de carte : supprime les lumieres ponctuelles et les pieces.
    void clearMapLights();
    void applyToShader(Shader* shader);
    void update();

    // ── Pièces intérieures (filtrage lumière) ────────────────────────────
    // addRoom : déclare une pièce (bornes monde de l'intérieur), retourne
    // son index. setLightRoom : affecte une lumière (index addPointLight) à
    // une pièce (index addRoom) — cette lumière n'éclaire alors que les
    // fragments de CETTE pièce (la lumière ne traverse pas les murs).
    // -1 / non affectée = lumière libre (extérieure). MAX_ROOMS max.
    int  addRoom(const glm::vec3& roomMin, const glm::vec3& roomMax);
    void setLightRoom(size_t lightIndex, int roomIndex);

    // Attache la lampe torche a une position monde explicite (ex: os de tete
    // du modele anime). La direction reste celle du joueur (borrowed).
    // A appeler APRES update() pour ecraser la position par defaut (yeux).
    void setFlashlightPosition(const glm::vec3& position);

    // Couleur/intensite de la spot du soleil (jaune chaud le jour, blanc-gris la nuit).
    void setSunSpotColor(const glm::vec3& diffuse);

    // Spot du soleil (uniform "sunSpot") : de `from` vers `target`.
    void aimSunSpot(const glm::vec3& from, const glm::vec3& target);

    // ── Soleil (lumière directionnelle) ───────────────────────────────
    // L'ambiance est propre à chaque carte : le MapLoader lit la clé "sun"
    // du JSON (direction + couleurs RGB) et la Scene appelle setDirLight.
    void setDirLight(const DirLight& light) { m_dirLight = light; ++m_frame; }
    const DirLight& getDirLight() const { return m_dirLight; }

    // ── Brouillard (exponentiel au carre) ─────────────────────────────
    // Applique par tous les shaders lumineux (uniforms fogColor/fogDensity).
    // getFogVisibility() = distance ou le brouillard atteint ~95 % : sert
    // aussi de portee de vue a la creature.
    void setFog(const glm::vec3& color, float density) { m_fogColor = color; m_fogDensity = density; ++m_frame; }
    float getFogVisibility() const { return m_fogDensity > 1e-5f ? 1.7321f / m_fogDensity : 1e9f; }

    // Accès (lecture) aux sources de lumière : utilisé par Scene pour le rendu
    // instancié des cubes de lumière.
    const std::vector<LightSource*>& getLightSources() const { return m_lightSources; }

private:

    // Locations GL des uniforms lumiere, resolues UNE SEULE FOIS par shader
    // (cle = id du programme GL) au premier applyToShader(). Le hot path
    // n'utilise alors que des GLint : plus aucun hash/compare de string.
    // Valide car les shaders (ShaderManager) sont crees une fois au demarrage
    // et ne sont jamais detruits/recrees en cours de jeu.
    struct LightLocations {
        int numberLightSources = -1;
        int position[Constants::Shader::MAX_LIGHTS_SOURCES] = {};
        int ambient[Constants::Shader::MAX_LIGHTS_SOURCES] = {};
        int diffuse[Constants::Shader::MAX_LIGHTS_SOURCES] = {};
        int specular[Constants::Shader::MAX_LIGHTS_SOURCES] = {};
        int constant[Constants::Shader::MAX_LIGHTS_SOURCES] = {};
        int linear[Constants::Shader::MAX_LIGHTS_SOURCES] = {};
        int quadratic[Constants::Shader::MAX_LIGHTS_SOURCES] = {};
        int dirDirection = -1;
        int dirAmbient = -1;
        int dirDiffuse = -1;
        int dirSpecular = -1;
        int fogColor = -1;
        int fogDensity = -1;
        int roomCount = -1;
        int roomMin[Constants::Shader::MAX_ROOMS] = {};
        int roomMax[Constants::Shader::MAX_ROOMS] = {};
        int lightRoom[Constants::Shader::MAX_LIGHTS_SOURCES] = {};
    };
    const LightLocations& ensureLightLocations(Shader* shader);

    static constexpr int MAX_POINT_LIGHTS = Constants::Shader::MAX_LIGHTS_SOURCES;
    std::vector<LightSource*> m_lightSources;                 // vues (ordre d'ajout, cf. getLightSources)
    std::vector<std::unique_ptr<LightSource>> m_ownedLights;  // possession
    std::unordered_map<unsigned int, LightLocations> m_lightLocations; // par id de programme GL
    std::unique_ptr<Spotlight> m_flashlight;
    std::unique_ptr<Spotlight> m_sunSpot;
    Player* m_player;
    struct DirLight m_dirLight;
    // Envoi des uniforms une seule fois par frame et par programme GL (cle = id) :
    // applyToShader est appele a chaque draw mais les valeurs ne changent qu'en update().
    unsigned long long m_frame = 1;
    std::unordered_map<unsigned int, unsigned long long> m_lastApplied;
    glm::vec3 m_fogColor{ 0.22f, 0.24f, 0.28f };
    float m_fogDensity = 0.08f;

    // Bornes (monde) des pièces intérieures + affectation lumière → pièce
    // (-1 = libre), alignée sur m_lightSources (voir addRoom/setLightRoom).
    std::vector<glm::vec3> m_roomMin;
    std::vector<glm::vec3> m_roomMax;
    std::vector<int>       m_lightRoom;
};