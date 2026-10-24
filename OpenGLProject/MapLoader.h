#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <glm/glm.hpp>

#include "Vertex.h"

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
//    "gableThickness" (m, défaut 0) = épaisseur du pignon (mettre l'épaisseur des murs) :
//    deux faces, extérieure dans le plan des murs et intérieure en retrait.
//    "gableInset" (m, défaut 0) = retrait du pignon par rapport au bout du toit : à
//    régler sur le débord de toiture pour que le pignon soit dans le plan des murs ;
//    "gableBase" (y local, défaut -thickness) = hauteur de la base du pignon (haut
//    des murs). Leur UV est en repère monde : briques alignées avec les murs.
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
//  - "models" : [ { "path": "./res/maps/x/x.obj", "position": [x,y,z], "yaw": deg, "scale": s } ]
//    modele OBJ (v/vt + f, quads/n-gones triangules, normales plates calculees), une
//    partie par couple (objet "o", materiau "usemtl") ; le .mtl (mtllib) fournit map_Kd
//    (texture, chemin relatif au .mtl), Kd (couleur) et d (opacite). Collision precise par
//    cellules de 4 m (pas de BVH de boites).
//  - "trees"  : arbres (res/models/trees/tree_<type>.obj, base en (0,0,0)).
//    Collision "mesh" systématique : la hitbox suit les triangles réels
//    (tronc ET feuillage). Le mesh d'un type est chargé une seule fois.
//    "scale" = échelle horizontale (et verticale par défaut) ; "scaleY" =
//    échelle verticale seule (hauteur de l'arbre).
//  - "leaves" (même fichier que "trees") : feuilles mortes au sol autour de
//    chaque arbre, générées à la volée depuis une SEED (toujours aux mêmes
//    endroits, rien de codé en dur dans la carte) :
//    { "seed": 1337, "perTree": 30, "radius": 3.5, "size": [0.35, 0.7] }
//    Planches res/models/trees/leaf_sheet_<0..2>.png (alpha), sans collision.
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
    Textured,   // modele OBJ : texture/couleur/alpha propres a la partie (MapPart::texturePath/color/alpha)
    Tree,       // arbres ("trees") : palette tree_palette.png (cube/severallights)
};

// Une partie statique de la carte : pavé droit ou toit à deux pans.
struct MapPart {
    std::string           name;
    MapMaterial           material = MapMaterial::BrickWall;
    glm::mat4             model{ 1.0f };   // translate · rotate (pas d'échelle
                                           // : les meshs sont à taille réelle)
    bool                  triangleCollision = false;  // "collision": "mesh"
    std::string           texturePath;      // modele OBJ : map_Kd du .mtl ("" = blanc teinte par color)
    glm::vec3             color{ 1.0f };    // modele OBJ : Kd du .mtl
    float                 alpha = 1.0f;     // modele OBJ : d du .mtl (< 1 = vitre transparente)
    std::vector<glm::vec3> soup;  // modèle OBJ : triangles MONDE pour la collision précise (vidé après load)
    std::shared_ptr<Mesh> mesh;   // partagé entre les instances d'un même arbre
    glm::vec3             tint{ 1.0f };   // arbres : delta de couleur par arbre (seed)
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

// Brouillard de la carte (cle JSON "fog", racine uniquement, optionnelle) :
// { "fog": { "color": [r,g,b], "density": 0.08 } } ; 0 = pas de brouillard.
struct MapFog {
    glm::vec3 color{ 0.22f, 0.24f, 0.28f };
    float density = 0.08f;
};

// Tete de fropy dans le ciel (cle JSON "sunHead", racine) : soleil jaune le jour,
// lune blanc-gris la nuit. distance = eloignement depuis le centre du village
// (hauteur ~ 0.94 x distance), scale = echelle du modele, spotIntensity = force
// de la spot qui eclaire la creature (couleur de la spot = color x intensite).
struct MapSunHead {
    glm::vec3 color{ 1.0f, 0.92f, 0.25f };
    float distance = 210.0f;
    float scale = 14.0f;
    float spotIntensity = 6.0f;
};

// Bouton interactif (cle JSON "buttons") : fropy qui tourne (modele fropy_low_poly) ;
// la Scene detecte le bouton vise et transmet son nom quand on appuie sur E.
// { "buttons": [ { "name": "start", "label": "Lancer la partie",
//                  "center": [x,y,z], "scale": 0.45 } ] }
struct MapButton {
    std::string name;
    std::string label;
    glm::vec3 position{ 0.0f };   // centre (monde) du fropy tournant
    float scale = 0.45f;          // echelle du modele fropy
    bool enabled = true;          // false : ni dessine ni visable (cf. Scene::setButtonEnabled)
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
    const MapFog& getFog() const { return m_fog; }
    const MapSunHead& getSunHead() const { return m_sunHead; }
    // Dossier du cubemap ("skybox", racine) ; vide = aucun ciel.
    const std::string& getSkybox() const { return m_skybox; }
    // Portee de vue de la creature en m ("creature": {"sightRange": n}) ; 0 = celle du brouillard.
    float getCreatureSightRange() const { return m_creatureSight; }
    // "creature": {"enabled": false} : pas de fropy sur cette carte (lobby).
    bool isCreatureEnabled() const { return m_creatureEnabled; }
    const std::vector<MapButton>& getButtons() const { return m_buttons; }
    std::vector<MapButton>& getButtons() { return m_buttons; }
    // "creature": {"patrol": {"min": [x,z], "max": [x,z]}} : zone des rondes ; absente = défaut Creature.
    bool hasPatrolBounds() const { return m_hasPatrol; }
    glm::vec2 getPatrolMin() const { return m_patrolMin; }
    glm::vec2 getPatrolMax() const { return m_patrolMax; }

    const std::vector<MapRoom>&  getRooms()  const { return m_rooms; }
    const std::vector<MapLight>& getLights() const { return m_lights; }

    // Feuilles au sol : un mesh (repère monde) par planche de texture, ou
    // nullptr s'il n'y en a pas.
    static constexpr int LEAF_SHEETS = 3;
    const Mesh* getLeafMesh(int sheet) const { return m_leafMeshes[sheet].get(); }

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
    MapFog                            m_fog;
    MapSunHead                        m_sunHead;
    std::string                       m_skybox;
    float                             m_creatureSight = 0.0f;
    bool                              m_creatureEnabled = true;
    bool                              m_hasPatrol = false;
    glm::vec2                         m_patrolMin{ 0.0f }, m_patrolMax{ 0.0f };
    std::vector<MapButton>            m_buttons;
    std::vector<MapRoom>              m_rooms;
    std::vector<MapLight>             m_lights;
    std::vector<MapSpawn>             m_spawns;

    // Feuilles : géométrie accumulée pendant le chargement, meshes finaux
    // construits à la fin de load().
    std::vector<Vertex>               m_leafVerts[LEAF_SHEETS];
    std::vector<unsigned int>         m_leafIdx[LEAF_SHEETS];
    std::unique_ptr<Mesh>             m_leafMeshes[LEAF_SHEETS];
    unsigned                          m_treeCounter = 0;   // graine par arbre
};
