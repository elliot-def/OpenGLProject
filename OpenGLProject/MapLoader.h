#pragma once

#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>

// Déclarations anticipées
class Mesh;
class Door;
class CollisionManager;

// ─────────────────────────────────────────────────────────────────────────────
// MapLoader : charge une carte 3D depuis un fichier JSON.
//
// Schéma du JSON (unités en mètres, Y vers le haut, sol en y=0) :
// {
//   "includes": [ { "path": "house.json", "name": "...",
//                   "position": [x,y,z], "yaw": deg } ],
//   "boxes": [ { "name": "...", "center": [x,y,z], "size": [sx,sy,sz],
//                "rotation": [rx,ry,rz],
//                "material": "brickwall|container|parquet|leaf|grey_roof|riet" } ],
//   "roof":  { "name": "...", "center": [x,y,z], "span": [sx,sz], "height": h,
//              "thickness": t, "material": "...", "gableMaterial": "..." },
//   "doors": [ { "name": "...", "hinge": [x,y,z], "yaw": deg } ]
// }
//  - "includes": instancie d'autres cartes (factorisation : ex. N fois la même
//    maison). "path" tel quel ou relatif au fichier appelant ; chaque
//    exemplaire est placé par "position" (translation) et "yaw" (degrés autour
//    de Y, cumulés avec ceux de la carte parente). Récursif : détection de
//    cycles et profondeur max (l'include fautif est ignoré avec un warning),
//    tandis qu'un fichier absent/JSON invalide fait échouer le chargement.
//  - "boxes"  : pavé droit (sol, mur, linteau...). "rotation" en degrés (Euler
//    XYZ, défaut [0,0,0]) ; UV projetés en repère monde selon la normale
//    dominante de la face (texture 1×/m, continue d'une boîte à l'autre).
//  - "roof"   : toit à deux pentes = 2 dalles jointives au faîtage (pignons
//    compris), base rectangulaire "span" × "span.y", faîtage à "height" au-
//    dessus de "center", épaisseur verticale "thickness" (défaut 0.15 m).
//    "material" = couverture (les dalles), "gableMaterial" = pignons
//    (défaut "brickwall" : la façade reste en briques même si la couverture
//  - "collision": "box" (défaut) = cube englobant (AABB, pour les pavés) ;
//    "mesh" = collision triangles, la hitbox suit la géométrie réelle (pentu
//    du toit...) — à mettre sur le "roof".
//  - "doors"  : portes interactives (classe Door) — largeur/hauteur/épaisseur
//    surchargables par "width"/"height"/"thickness" (défauts constants/door.h).
//
// Le loader possède les meshs et les portes qu'il crée ; la Scene n'en garde
// que des vues non-propriétaires (même convention que ModelLoader). Les
// collisions statiques sont enregistrées dans le CollisionManager et le BVH
// est construit à la fin du load().
// ─────────────────────────────────────────────────────────────────────────────

// Matériau d'une partie de la carte (clé JSON "material").
enum class MapMaterial {
    BrickWall,  // "brickwall" : brique + normal mapping (shader cube/wall)
    Container,  // "container" : caisse en bois (shader cube/severallights)
    Parquet,    // "parquet"   : parquet (wood shutter), sol intérieur (severallights)
    Leaf,       // "leaf"      : terre/feuilles, sol extérieur (cube/severallights)
    GreyRoof,   // "grey_roof" : toit gris (shader cube/severallights)
    Riet,       // "riet"      : tapis d'entrée (shader cube/severallights)
};

// Une partie statique de la carte : pavé droit ou toit à deux pans.
struct MapPart {
    std::string           name;
    MapMaterial           material = MapMaterial::BrickWall;
    glm::mat4             model{ 1.0f };   // translate · rotate (pas d'échelle
                                           // : les meshs sont à taille réelle)
    bool                  triangleCollision = false;  // "collision": "mesh"
    std::unique_ptr<Mesh> mesh;
};

class MapLoader {
public:
    // collisionManager : requis pour enregistrer les collisions statiques.
    explicit MapLoader(CollisionManager* collisionManager);
    ~MapLoader();

    // Construit les meshs + portes depuis le JSON et enregistre les collisions.
    // Retourne false si le fichier est absent ou le JSON invalide (le contenu
    // déjà chargé est alors abandonné).
    bool load(const std::string& jsonPath);

    // Parties de la carte (détenu par le loader).
    const std::vector<MapPart>& getParts() const { return m_parts; }

    // Portes interactives (détenues par le loader).
    std::vector<std::unique_ptr<Door>>& getDoors() { return m_doors; }

private:
    // Charge un fichier de carte, récursivement pour les "includes".
    // transform + yawOffset : repère local → monde de la carte appelée ;
    // prefix : préfixe des noms de parties ; stack : chemins canoniques en
    // cours (détection de cycles).
    bool loadMapFile(const std::string& jsonPath, const glm::mat4& transform,
                     float yawOffsetDeg, unsigned depth, const std::string& prefix,
                     std::vector<std::string>& stack);

    CollisionManager* m_collisionManager;

    std::vector<MapPart>              m_parts;
    std::vector<std::unique_ptr<Door>> m_doors;
};
