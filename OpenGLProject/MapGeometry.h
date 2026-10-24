#pragma once

#include <memory>
#include <glm/glm.hpp>

class Mesh;

// ─────────────────────────────────────────────────────────────────────────────
// Generateurs de geometrie des cartes (MapLoader) : pave droit, toit a deux pans,
// pignons. UV en metres x kUvScale (une repetition de texture tous les 1/kUvScale m).
// ─────────────────────────────────────────────────────────────────────────────

// kUvScale < 1 agrandit les textures : 0.4 = une repetition tous les 2.5 m.
inline constexpr float kUvScale = 0.4f;

// Pave droit centre sur l'origine (taille reelle) ; UV projetes en repere monde
// via `model` (textures continues d'une boite a l'autre).
std::unique_ptr<Mesh> buildBoxMesh(const glm::vec3& size, const glm::mat4& model);

// Toit a deux pentes epaisses (2 dalles jointives au faitage + caps d'extremite).
std::unique_ptr<Mesh> buildRoofMesh(const glm::vec2& span, float height, float thickness);

// Pignons sous le toit : en retrait de `inset`, base en `baseLocalY`, epaisseur de mur
// `wallThickness` (face interieure), UV monde via `model`.
std::unique_ptr<Mesh> buildGablesMesh(const glm::vec2& span, float height, float thickness,
                                      float inset, float baseLocalY, float wallThickness,
                                      const glm::mat4& model);
