#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// ArmsRenderer : helper dedie au rendu first-person des avant-bras rigues.
//
// Isolé de Game.cpp / Player.cpp pour :
//   1. Eviter la pollution visuelle du draw() de Game.
//   2. Concentrer toute la math bone palette + camera-relative dans un seul TU.
//   3. Permettre le swap d'implementation (procedural / shader / opaque-fallback).
//
// API minimale en v1 : une seule fonction statique drawFP().
// ─────────────────────────────────────────────────────────────────────────────

class Camera;
class ShaderManager;
class LightManager;
class Model;
class Animator;

class ArmsRenderer {
public:
    // Rend les avant-bras en mode first-person : skinned shader -> bone palette ->
    // camera-relative model matrix -> draw direct de chaque mesh.
    // Aucun effet si l'un des pointeurs est nullptr (l'appelant decide).
    // lightManager peut etre nullptr => pas de lumieres dynamiques sur les bras.
    static void drawFP(Camera* camera,
                       ShaderManager* shaderManager,
                       LightManager* lightManager,
                       Model* armsModel,
                       Animator* armsAnimator);
};
