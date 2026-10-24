#pragma once

#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>

#include "Vertex.h"

class Mesh;

// ─────────────────────────────────────────────────────────────────────────────
// Chargeurs OBJ des cartes (sans Assimp) :
//  - loadObjMesh : OBJ simple v/vt/vn triangule (arbres) -> un Mesh ;
//  - loadObjParts : OBJ + MTL (batiment) -> une partie par couple objet/materiau,
//    normales plates, UV du fichier, soupe de triangles monde pour la collision.
// ─────────────────────────────────────────────────────────────────────────────

struct ObjMaterial {
    std::string texture;
    glm::vec3 kd{ 1.0f };
    float alpha = 1.0f;
};

struct ObjPart {
    std::string name;
    ObjMaterial mat;
    std::vector<Vertex> verts;
    std::vector<unsigned int> idx;
    std::vector<glm::vec3> soup;
};

// OBJ simple (v/vt/vn, faces triangulees) ; nullptr si absent/vide.
std::shared_ptr<Mesh> loadObjMesh(const std::string& path);

// OBJ + MTL transforme par `m` ; vide si le fichier est absent.
std::vector<ObjPart> loadObjParts(const std::string& path, const glm::mat4& m);
