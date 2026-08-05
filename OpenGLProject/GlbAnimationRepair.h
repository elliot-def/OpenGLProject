#pragma once

#include <assimp/scene.h>
#include <string>

// Lit le fichier GLB et restaure les clés de rotation corrompues par Assimp
// 5.4.x. Appelé par Model::loadModel() avant processNode().
// Retourne true si au moins un canal a été réparé.
bool patchGlbAnimationRotations(const aiScene* scene, const std::string& path);
