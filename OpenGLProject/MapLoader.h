#pragma once

#include <map>
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
//   "tvs":   [ { "name": "...", "center": [x,y,z], "rotation": [rx,ry,rz],
//                "width": w, "height": h } ],
//   "doors": [ { "name": "...", "hinge": [x,y,z], "yaw": deg } ],
//   "trees": [ { "type": 0..5, "position": [x,y,z], "yaw": deg, "scale": s } ]
// }
//  - "includes": instancie d'autres cartes (factorisation : ex. N fois la même
//    maison). "path" tel quel ou relatif au fichier appelant ; chaque
//    exemplaire est placé par "position" (translation) et "yaw" (degrés autour
//    de Y, cumulés avec ceux de la carte parente). Récursif : détection de
//    cycles et profondeur max (l'include fautif est ignoré avec un warning),
//    tandis qu'un fichier absent/JSON invalide fait échouer le chargement.
//  - "boxes"  : pavé droit (sol, mur, linteau...). "rotation" en degrés (Euler
//    XYZ, défaut [0,0,0]) ; UV projetés en repère monde selon la normale
//    dominante de la face (texture 1×/m, continue d'une boîte à l'autre, y
//    compris pour les boîtes tournées : la normale est transformée par la
//    matrice modèle avant de choisir les axes UV).
//  - "roof"   : toit à deux pentes = 2 dalles jointives au faîtage (pignons
//    compris), base rectangulaire "span" × "span.y", faîtage à "height" au-
//    dessus de "center", épaisseur verticale "thickness" (défaut 0.15 m).
//    "material" = couverture (les dalles), "gableMaterial" = pignons
//    (défaut "brickwall" : la façade reste en briques même si la couverture
//  - "collision": "box" (défaut) = cube englobant (AABB, pour les pavés) ;
//    "mesh" = collision triangles, la hitbox suit la géométrie réelle (pentu
//    du toit...) — à mettre sur le "roof".
//  - "tvs"   : écran de télévision (sur un meuble = un "box"). "center" =
//    centre de l'écran, "rotation" comme les "boxes" (l'écran est un quad dans
//    le plan XY local, normale +Z) ; "width"/"height" en mètres. Le loader ne
//    stocke que le placement (pas de mesh, pas de collision) : la Scene crée
//    les TvScreen (easter egg DVD animé, visible de près).
//  - "doors"  : portes interactives (classe Door) — largeur/hauteur/épaisseur
//    surchargables par "width"/"height"/"thickness" (défauts constants/door.h).
//
//  - "trees"  : arbres (res/models/trees/tree_<type>.obj, base en (0,0,0)).
//    Collision "mesh" systématique : la hitbox suit les triangles réels
//    (tronc ET feuillage). Le mesh d'un type est chargé une seule fois.
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
    Tree,       // arbres ("trees") : palette tree_palette.png (cube/severallights)
};

// Une partie statique de la carte : pavé droit ou toit à deux pans.
struct MapPart {
    std::string           name;
    MapMaterial           material = MapMaterial::BrickWall;
    glm::mat4             model{ 1.0f };   // translate · rotate (pas d'échelle
                                           // : les meshs sont à taille réelle)
    bool                  triangleCollision = false;  // "collision": "mesh"
    std::shared_ptr<Mesh> mesh;   // partagé entre les instances d'un même arbre
};

// Placement d'un écran de télévision (clé JSON "tvs") : quad centré dans le
// plan XY local (normale +Z), largeur/hauteur en mètres. Pas de mesh ni de
// collision — la Scene crée les TvScreen (easter egg DVD) à partir de ça.
struct MapScreen {
    std::string name;
    glm::mat4   model{ 1.0f };   // translate · rotate (comme MapPart::model)
    float       width  = 1.0f;
    float       height = 0.6f;
};

// ── Ambiance / ensolleilement (cle JSON "sun") ──────────────────────────────
// Parametres du SOLEIL de la carte : direction du rayonnement de la lumiere
// directionnelle + couleurs RGB de l'ambiance (ambiante / diffuse / speculaire).
// C'est LE reglage d'ambiance par map : chaque carte configure ses couleurs.
// Lu uniquement au niveau racine (pas dans les "includes") ; cles optionnelles.
// {
//   "sun": { "direction": [x, y, z],  // vers le bas, ex [-0.3, -1, -0.2]
//            "ambient":  [r, g, b],   // couleur ambiante
//            "diffuse":  [r, g, b],   // lumiere directe (soleil)
//            "specular": [r, g, b] }  // reflets
// }
struct MapSun {
    glm::vec3 direction{ -0.3f, -1.0f, -0.2f };
    glm::vec3 ambient  { 0.55f, 0.55f, 0.48f };
    glm::vec3 diffuse  { 1.5f, 1.5f, 1.4f };
    glm::vec3 specular { 0.6f, 0.6f, 0.6f };
};

// ── Lumières / pièces / spawns (clés JSON "lights", "rooms", "spawns") ──────
// Toutes les positions passent par la transform des "includes" : une maison
// décrite une fois dans house.json porte ses lumières et sa pièce partout.
// {
//   "rooms":  [ { "name": "salon", "min": [x,y,z], "max": [x,y,z] } ],
//   "lights": [ { "position": [x,y,z], "room": "salon",   // room optionnel
//                 "ambient": [r,g,b], "diffuse": [r,g,b], "specular": [r,g,b],
//                 "attenuation": [constant, linear, quadratic],
//                 "color": [r,g,b] } ],                    // couleur du cube visible
//   "spawns": [ { "name": "creature", "position": [x,y,z] } ]
// }
// "room" = nom d'une pièce du MÊME fichier : la lumière n'éclaire alors que
// cette pièce (cf. LightManager::addRoom/setLightRoom). Sans "room" = libre.
struct MapRoom {
    std::string name;        // préfixé par l'include
    glm::vec3   min{ 0.0f }; // AABB monde
    glm::vec3   max{ 0.0f };
};

struct MapLight {
    glm::vec3   position{ 0.0f };
    std::string room;        // nom préfixé de la pièce, "" = libre
    glm::vec3   ambient { 0.1f };
    glm::vec3   diffuse { 1.0f };
    glm::vec3   specular{ 1.0f };
    glm::vec3   attenuation{ 1.0f, 0.09f, 0.032f };
    glm::vec3   color{ 1.0f };
};

struct MapSpawn {
    std::string name;        // préfixé par l'include
    glm::vec3   position{ 0.0f };
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

    // Placements des écrans ("tvs"), détenus par le loader.
    const std::vector<MapScreen>& getScreens() const { return m_screens; }

    // Soleil / ambiance de la carte (clé "sun" du JSON) — cf. MapSun.
    const MapSun& getSun() const { return m_sun; }

    const std::vector<MapRoom>&  getRooms()  const { return m_rooms; }
    const std::vector<MapLight>& getLights() const { return m_lights; }

    // Position du spawn nommé (nom complet, préfixe d'include compris).
    // Renvoie fallback si absent.
    glm::vec3 getSpawn(const std::string& name, const glm::vec3& fallback) const {
        for (const auto& s : m_spawns) if (s.name == name) return s.position;
        return fallback;
    }

private:
    // Charge un fichier de carte, récursivement pour les "includes".
    // transform + yawOffset : repère local → monde de la carte appelée ;
    // prefix : préfixe des noms de parties ; stack : chemins canoniques en
    // cours (détection de cycles).
    bool loadMapFile(const std::string& jsonPath, const glm::mat4& transform,
                     float yawOffsetDeg, unsigned depth, const std::string& prefix,
                     std::vector<std::string>& stack);

    CollisionManager* m_collisionManager;
    std::map<int, std::shared_ptr<Mesh>> m_treeMeshes;   // cache par "type"

    std::vector<MapPart>              m_parts;
    std::vector<MapScreen>            m_screens;
    std::vector<std::unique_ptr<Door>> m_doors;
    MapSun                            m_sun;
    std::vector<MapRoom>              m_rooms;
    std::vector<MapLight>             m_lights;
    std::vector<MapSpawn>             m_spawns;
};
