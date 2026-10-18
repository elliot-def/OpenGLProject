#include "MapLoader.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdio>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include <glm/gtc/matrix_transform.hpp>

#include "Mesh.h"
#include "Door.h"
#include "CollisionManager.h"
#include "Log.h"

#include "constants/door.h"

// ── Lecture JSON ─────────────────────────────────────────────────────────────

// Lit un tableau [x,y,z] ; renvoie fallback si absent ou mal formé.
static glm::vec3 readVec3(const json& j, const glm::vec3& fallback) {
    if (!j.is_array() || j.size() != 3) return fallback;
    return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
}

// Lit un tableau [x,z] ; renvoie fallback si absent ou mal formé.
static glm::vec2 readVec2(const json& j, const glm::vec2& fallback) {
    if (!j.is_array() || j.size() != 2) return fallback;
    return glm::vec2(j[0].get<float>(), j[1].get<float>());
}

// Matériau depuis la chaîne JSON ("brickwall" par défaut).
static MapMaterial readMaterial(const json& j) {
    if (!j.is_string()) return MapMaterial::BrickWall;
    const std::string name = j.get<std::string>();
    if (name == "container") return MapMaterial::Container;
    if (name == "parquet")   return MapMaterial::Parquet;
    if (name == "leaf")      return MapMaterial::Leaf;
    if (name == "grey_roof") return MapMaterial::GreyRoof;
    if (name == "riet")      return MapMaterial::Riet;
    return MapMaterial::BrickWall;
}

// Matrice de rotation Euler XYZ en degrés.
static glm::mat4 rotationMatrix(const glm::vec3& rotDeg) {
    glm::mat4 m(1.0f);
    m = glm::rotate(m, glm::radians(rotDeg.x), glm::vec3(1.0f, 0.0f, 0.0f));
    m = glm::rotate(m, glm::radians(rotDeg.y), glm::vec3(0.0f, 1.0f, 0.0f));
    m = glm::rotate(m, glm::radians(rotDeg.z), glm::vec3(0.0f, 0.0f, 1.0f));
    return m;
}

// ── Génération de géométrie (UV en mètres : la texture se répète 1×/m) ──────

static void pushQuad(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices,
                     const glm::vec3& v0, const glm::vec3& v1,
                     const glm::vec3& v2, const glm::vec3& v3,
                     float uSpan, float vSpan) {
    // Normale de face depuis le winding (v0,v1,v2) — faces CCW vues de l'extérieur.
    const glm::vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
    const unsigned int base = static_cast<unsigned int>(vertices.size());

    vertices.push_back(Vertex(v0.x, v0.y, v0.z, normal.x, normal.y, normal.z, 0.0f, 0.0f));
    vertices.push_back(Vertex(v1.x, v1.y, v1.z, normal.x, normal.y, normal.z, uSpan, 0.0f));
    vertices.push_back(Vertex(v2.x, v2.y, v2.z, normal.x, normal.y, normal.z, uSpan, vSpan));
    vertices.push_back(Vertex(v3.x, v3.y, v3.z, normal.x, normal.y, normal.z, 0.0f, vSpan));

    indices.insert(indices.end(), { base, base + 1, base + 2,
                                    base + 2, base + 3, base });
}

static void pushTriangle(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices,
                         const glm::vec3& v0, const glm::vec2& uv0,
                         const glm::vec3& v1, const glm::vec2& uv1,
                         const glm::vec3& v2, const glm::vec2& uv2) {
    const glm::vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
    const unsigned int base = static_cast<unsigned int>(vertices.size());

    vertices.push_back(Vertex(v0.x, v0.y, v0.z, normal.x, normal.y, normal.z, uv0.x, uv0.y));
    vertices.push_back(Vertex(v1.x, v1.y, v1.z, normal.x, normal.y, normal.z, uv1.x, uv1.y));
    vertices.push_back(Vertex(v2.x, v2.y, v2.z, normal.x, normal.y, normal.z, uv2.x, uv2.y));

    indices.insert(indices.end(), { base, base + 1, base + 2 });
}

// Pavé droit centré sur l'origine, taille réelle (pas de mise à l'échelle
// dans la matrice modèle). 6 faces × 4 sommets, UV en mètres.
static std::unique_ptr<Mesh> buildBoxMesh(const glm::vec3& size, const glm::mat4& model) {
    const glm::vec3 h = size * 0.5f;

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    vertices.reserve(24);
    indices.reserve(36);

    pushQuad(vertices, indices,
             {-h.x, -h.y,  h.z}, { h.x, -h.y,  h.z}, { h.x,  h.y,  h.z}, {-h.x,  h.y,  h.z},
             size.x, size.y);  // +Z
    pushQuad(vertices, indices,
             {-h.x,  h.y, -h.z}, { h.x,  h.y, -h.z}, { h.x, -h.y, -h.z}, {-h.x, -h.y, -h.z},
             size.x, size.y);  // -Z
    pushQuad(vertices, indices,
             { h.x, -h.y,  h.z}, { h.x, -h.y, -h.z}, { h.x,  h.y, -h.z}, { h.x,  h.y,  h.z},
             size.z, size.y);  // +X
    pushQuad(vertices, indices,
             {-h.x, -h.y, -h.z}, {-h.x, -h.y,  h.z}, {-h.x,  h.y,  h.z}, {-h.x,  h.y, -h.z},
             size.z, size.y);  // -X
    pushQuad(vertices, indices,
             {-h.x,  h.y,  h.z}, { h.x,  h.y,  h.z}, { h.x,  h.y, -h.z}, {-h.x,  h.y, -h.z},
             size.x, size.z);  // +Y (dessus)
    pushQuad(vertices, indices,
             {-h.x, -h.y, -h.z}, { h.x, -h.y, -h.z}, { h.x, -h.y,  h.z}, {-h.x, -h.y,  h.z},
             size.x, size.z);  // -Y (dessous)

    // UV projetés en repère monde (selon la normale dominante de la face) :
    // deux faces coplanaires de boîtes différentes deviennent texel-identiques
    // → pas de z-fighting visible aux jonctions (le culling est désactivé), et
    // la texture est continue d'une boîte à l'autre sur toute la carte.
    // ATTENTION : la normale doit être TRANSFORMÉE par la matrice modèle avant
    // de choisir les axes UV. Avec la normale locale, une boîte tournée (les
    // maisons instanciées avec "yaw" dans village.json) choisit les mauvais
    // axes : un axe UV devient constant sur la face → la texture s'écrase sur
    // une bande de 1 texel (les murs en briques "disparaissent").
    for (auto& v : vertices) {
        const glm::vec3 world = glm::vec3(model * glm::vec4(v.getPositions(), 1.0f));
        const glm::vec3 n = glm::abs(glm::mat3(model) * v.getNormals());
        glm::vec2 uv;
        if (n.x >= n.y && n.x >= n.z) uv = glm::vec2(world.z, world.y);
        else if (n.y >= n.z)          uv = glm::vec2(world.x, world.z);
        else                          uv = glm::vec2(world.x, world.y);
        v = Vertex(v.getPositions(), v.getNormals(), v.getColor(), uv);
    }

    return std::make_unique<Mesh>(vertices, indices,
        (unsigned int)VertexAttribute::POSITION |
        (unsigned int)VertexAttribute::NORMAL |
        (unsigned int)VertexAttribute::TEXCOORD);
}

// Toit à deux pentes = 2 dalles inclinées jointives au faîtage, chacune avec
// une épaisseur `thickness` (verticale) : visu « couverture » au lieu du
// prisme plein. Le pignon est porté par buildGablesMesh.
//
// Par dalle (côté s, s=-1 ou +1, hauteur h) :
//   dessus  (s·hx, 0) → (0, h)
//   dessous (s·hx, -t) → (0, h-t)  (décalage vertical)
//   tranche d'égout verticale à x=s·hx entre y=0 et y=-t
//   2 tranches d'extrémité (caps) aux bouts z=±hz : la coupe de la dalle,
//   parallélogramme (s·hx,0)-(0,h)-(0,h-t)-(s·hx,-t) → la bordure de la
//   couverture reste TEXTURÉE et garde toute son épaisseur t jusqu'au faîtage
//   (sans ces caps, l'extrémité est ouverte : la dalle dépasse du pignon d'un
//   lambeau dont l'épaisseur visible tombe à 0 au faîtage, et ce bord n'a
//   aucune face → pas de texture).
// Les 2 dessus partagent l'arête du faîtage, les 2 dessous celle en (0,h-t) :
// le volume est fermé, aucune face coplanaire entre les 2 dalles.
static std::unique_ptr<Mesh> buildRoofMesh(const glm::vec2& span, float height,
                                           float thickness) {
    const float hx = span.x * 0.5f;
    const float hz = span.y * 0.5f;
    const float t  = thickness;

    const glm::vec3 A(-hx, 0.0f, -hz);   // coins de la base (y=0)
    const glm::vec3 B( hx, 0.0f, -hz);
    const glm::vec3 C( hx, 0.0f,  hz);
    const glm::vec3 D(-hx, 0.0f,  hz);
    const glm::vec3 R0(0.0f, height, -hz);   // extrémités du faîtage
    const glm::vec3 R1(0.0f, height,  hz);

    // Notation X2 = point X décalé vers le bas de l'épaisseur.
    const glm::vec3 down(0.0f, -t, 0.0f);
    const glm::vec3 A2 = A + down, B2 = B + down, C2 = C + down, D2 = D + down;
    const glm::vec3 R02 = R0 + down, R12 = R1 + down;

    const float slopeLen = std::sqrt(hx * hx + height * height);

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    vertices.reserve(40);   // 2 dalles × (8 sommets dessus/dessous + 4 égout
                            // + 8 sommets pour les 2 caps d'extrémité)
    indices.reserve(60);

    // Dalle -X
    pushQuad(vertices, indices, A, D, R1, R0, span.y, slopeLen);       // dessus
    pushQuad(vertices, indices, A2, R02, R12, D2, span.y, slopeLen);   // dessous
    pushQuad(vertices, indices, A, A2, D2, D, t, span.y);              // égout (-X)
    pushQuad(vertices, indices, A, R0, R02, A2, slopeLen, t);          // cap z=-hz
    pushQuad(vertices, indices, D, D2, R12, R1, slopeLen, t);          // cap z=+hz

    // Dalle +X
    pushQuad(vertices, indices, R1, C, B, R0, span.y, slopeLen);       // dessus
    pushQuad(vertices, indices, R02, B2, C2, R12, span.y, slopeLen);   // dessous
    pushQuad(vertices, indices, C, C2, B2, B, t, span.y);              // égout (+X)
    pushQuad(vertices, indices, B, B2, R02, R0, slopeLen, t);          // cap z=-hz
    pushQuad(vertices, indices, C, R1, R12, C2, slopeLen, t);          // cap z=+hz

    return std::make_unique<Mesh>(vertices, indices,
        (unsigned int)VertexAttribute::POSITION |
        (unsigned int)VertexAttribute::NORMAL |
        (unsigned int)VertexAttribute::TEXCOORD);
}

// Pignons : 2 triangles verticaux aux extrémités du faîtage (la façade
// continue les murs jusqu'au toit). Le triangle remplit exactement la zone
// SOUS le dessous des 2 dalles de couverture : base en y=-thickness, sommet
// en (0, height-thickness) — les arêtes suivent le dessous des dalles
// ((-hx,-t) → (0,h-t) → (hx,-t)). Les caps d'extrémité des dalles (cf.
// buildRoofMesh) couvrent au-dessus : chaque face est dans son plan, aucune
// coplanaire, et la coupe de la couverture reste visible et texturée.
// UV = coordonnées locales en mètres (u = x + hx, v = y + thickness).
static std::unique_ptr<Mesh> buildGablesMesh(const glm::vec2& span, float height,
                                             float thickness) {
    const float hx = span.x * 0.5f;
    const float hz = span.y * 0.5f;
    const float baseY = -thickness;

    const glm::vec3 A(-hx, baseY, -hz);   // base abaissée
    const glm::vec3 B( hx, baseY, -hz);
    const glm::vec3 C( hx, baseY,  hz);
    const glm::vec3 D(-hx, baseY,  hz);
    // Sommet sous le faîtage (dessous des dalles), pas au faîtage lui-même :
    // au-dessus, ce sont les caps des dalles (matériau couverture) qui
    // ferment, avec l'épaisseur t visible jusqu'en haut.
    const glm::vec3 R0(0.0f, height - thickness, -hz);
    const glm::vec3 R1(0.0f, height - thickness,  hz);

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    vertices.reserve(6);
    indices.reserve(6);

    pushTriangle(vertices, indices,
                 D, {0.0f, 0.0f}, C, {span.x, 0.0f},
                 R1, {hx, height});   // +Z
    pushTriangle(vertices, indices,
                 B, {span.x, 0.0f}, A, {0.0f, 0.0f},
                 R0, {hx, height});   // -Z

    return std::make_unique<Mesh>(vertices, indices,
        (unsigned int)VertexAttribute::POSITION |
        (unsigned int)VertexAttribute::NORMAL |
        (unsigned int)VertexAttribute::TEXCOORD);
}

// Charge un OBJ simple (v/vt/vn, faces v/vt/vn triangulées) en Mesh.
// nullptr si le fichier est absent ou vide.
static std::shared_ptr<Mesh> loadObjMesh(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return nullptr;

    std::vector<glm::vec3> pos, nor;
    std::vector<glm::vec2> uv;
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag == "v")       { glm::vec3 p; ss >> p.x >> p.y >> p.z; pos.push_back(p); }
        else if (tag == "vn") { glm::vec3 n; ss >> n.x >> n.y >> n.z; nor.push_back(n); }
        else if (tag == "vt") { glm::vec2 t; ss >> t.x >> t.y; uv.push_back(t); }
        else if (tag == "f") {
            std::string tok;
            while (ss >> tok) {
                int vi = 0, ti = 0, ni = 0;
                std::sscanf(tok.c_str(), "%d/%d/%d", &vi, &ti, &ni);
                if (vi < 1 || ti < 1 || ni < 1 || vi > (int)pos.size() ||
                    ti > (int)uv.size() || ni > (int)nor.size()) continue;
                const glm::vec3& p = pos[vi - 1];
                const glm::vec3& n = nor[ni - 1];
                const glm::vec2& t = uv[ti - 1];
                indices.push_back((unsigned int)vertices.size());
                vertices.push_back(Vertex(p.x, p.y, p.z, n.x, n.y, n.z, t.x, t.y));
            }
        }
    }
    if (vertices.empty()) return nullptr;
    return std::make_shared<Mesh>(vertices, indices,
        (unsigned int)VertexAttribute::POSITION |
        (unsigned int)VertexAttribute::NORMAL |
        (unsigned int)VertexAttribute::TEXCOORD);
}

// ── MapLoader ────────────────────────────────────────────────────────────────

// Profondeur maximale d'includes imbriqués (protection anti-boucle).
static constexpr unsigned MAX_INCLUDE_DEPTH = 8;

MapLoader::MapLoader(CollisionManager* collisionManager)
    : m_collisionManager(collisionManager) {
}

MapLoader::~MapLoader() = default;

bool MapLoader::load(const std::string& jsonPath) {
    m_parts.clear();
    m_screens.clear();
    m_doors.clear();
    m_rooms.clear();
    m_lights.clear();
    m_spawns.clear();

    std::vector<std::string> stack;
    if (!loadMapFile(jsonPath, glm::mat4(1.0f), 0.0f, 0, "", stack)) {
        m_parts.clear();
        m_screens.clear();
        m_doors.clear();
        m_rooms.clear();
        m_lights.clear();
        m_spawns.clear();
        return false;
    }

    // Collisions statiques (les portes gèrent leurs colliders dynamiques
    // côté Scene) puis construction du BVH du niveau. "collision": "mesh" →
    // triangles (la hitbox suit la géométrie : pente du toit...), sinon AABB.
    for (const auto& part : m_parts) {
        if (part.triangleCollision) {
            m_collisionManager->addStaticTriangleMesh(part.mesh.get(), part.model, part.name);
        } else {
            m_collisionManager->addStaticMesh(part.mesh.get(), part.model, part.name);
        }
    }
    m_collisionManager->buildBVH();

    LOG_INFO("[Map] Carte chargee : %s (%zu parties, %zu ecrans, %zu portes)",
             jsonPath.c_str(), m_parts.size(), m_screens.size(), m_doors.size());
    return true;
}

bool MapLoader::loadMapFile(const std::string& jsonPath, const glm::mat4& transform,
                            float yawOffsetDeg, unsigned depth, const std::string& prefix,
                            std::vector<std::string>& stack) {
    // Profondeur max : un include trop imbriqué est ignoré (fail-soft) plutôt
    // que de faire échouer toute la carte.
    if (depth > MAX_INCLUDE_DEPTH) {
        LOG_WARN("[Map] Profondeur d'includes trop grande, carte ignoree : %s", jsonPath.c_str());
        return true;
    }

    // Chemin : essayé tel quel (relatif au cwd de l'exécutable), sinon relatif
    // au dossier de la carte appelante (permet "house.json" entre deux cartes
    // d'un même dossier).
    std::filesystem::path path(jsonPath);
    if (!std::filesystem::exists(path) && !stack.empty()) {
        const std::filesystem::path sibling =
            std::filesystem::path(stack.back()).parent_path() / jsonPath;
        if (std::filesystem::exists(sibling)) path = sibling;
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("[Map] Fichier de carte introuvable : %s", jsonPath.c_str());
        return false;
    }

    // Cycle (A inclut A, ou A -> B -> A) : l'include est ignoré (fail-soft),
    // la carte parente est chargée sans lui.
    std::error_code ec;
    std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(path, ec);
    if (ec) canonicalPath = path;
    const std::string canonical = canonicalPath.generic_string();
    if (std::find(stack.begin(), stack.end(), canonical) != stack.end()) {
        LOG_WARN("[Map] Include cyclique ignore : %s", path.string().c_str());
        return true;
    }
    stack.push_back(canonical);

    bool ok = true;
    try {
        const json data = json::parse(file);

        // -1. Ambiance / ensolleilement : clé "sun" (racine uniquement, pas
        //     dans les includes) = direction + couleurs RGB de la lumière
        //     directionnelle. La Scene applique ça via LightManager::setDirLight.
        if (depth == 0 && data.contains("sun") && data["sun"].is_object()) {
            const json& sun = data["sun"];
            auto readVec3 = [&sun](const char* key, glm::vec3& out) {
                if (sun.contains(key) && sun[key].is_array() && sun[key].size() == 3) {
                    out = glm::vec3(sun[key][0].get<float>(),
                                    sun[key][1].get<float>(),
                                    sun[key][2].get<float>());
                }
            };
            readVec3("direction", m_sun.direction);
            readVec3("ambient",  m_sun.ambient);
            readVec3("diffuse",  m_sun.diffuse);
            readVec3("specular", m_sun.specular);
            LOG_INFO("[Map] Soleil: dir(%.2f, %.2f, %.2f) ambient(%.2f, %.2f, %.2f) diffuse(%.2f, %.2f, %.2f)",
                     m_sun.direction.x, m_sun.direction.y, m_sun.direction.z,
                     m_sun.ambient.r, m_sun.ambient.g, m_sun.ambient.b,
                     m_sun.diffuse.r, m_sun.diffuse.g, m_sun.diffuse.b);
        }

        // 0. Includes : instancie d'autres cartes (prefabs factorisés, ex. N
        //    fois la même maison), chacune placée par "position" (translation)
        //    et "yaw" (degrés autour de Y).
        if (data.contains("includes") && data["includes"].is_array()) {
            for (const auto& inc : data["includes"]) {
                const std::string incPath = inc.value("path", "");
                if (incPath.empty()) {
                    LOG_WARN("[Map] Include sans \"path\" ignore dans %s", path.string().c_str());
                    continue;
                }

                const float yaw = inc.value("yaw", 0.0f);
                const glm::vec3 offset = readVec3(inc.value("position", json()), glm::vec3(0.0f));
                const glm::mat4 incTransform =
                      transform
                    * glm::translate(glm::mat4(1.0f), offset)
                    * glm::rotate(glm::mat4(1.0f), glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));

                std::string incPrefix = inc.value("name", "");
                if (incPrefix.empty()) {
                    incPrefix = std::filesystem::path(incPath).stem().string();
                }

                if (!loadMapFile(incPath, incTransform, yawOffsetDeg + yaw,
                                 depth + 1, prefix + incPrefix + "/", stack)) {
                    ok = false;
                    break;
                }
            }
        }

        // 1. Boîtes (sol, murs, linteaux...)
        if (data.contains("boxes") && data["boxes"].is_array()) {
            for (const auto& box : data["boxes"]) {
                MapPart part;
                part.name     = prefix + box.value("name", "box");
                part.material = readMaterial(box.value("material", json()));
                part.triangleCollision = (box.value("collision", "box") == "mesh");
                part.model    = transform
                              * glm::translate(glm::mat4(1.0f),
                                   readVec3(box.value("center", json()), glm::vec3(0.0f)))
                              * rotationMatrix(readVec3(box.value("rotation", json()), glm::vec3(0.0f)));
                part.mesh     = buildBoxMesh(readVec3(box.value("size", json()), glm::vec3(1.0f)),
                                             part.model);
                m_parts.push_back(std::move(part));
            }
        }

        // 2. Toit à deux pentes épaisses (optionnel) : 2 parties distinctes —
        // couverture ("material", 2 dalles jointives au faîtage) et pignons
        // ("gableMaterial", défaut = bricks pour continuer les murs).
        // "thickness" = épaisseur verticale des dalles.
        // Même matrice modèle + même flag de collision → collisions
        // identiques à l'ancien prisme (contour extérieur conservé).
        if (data.contains("roof") && data["roof"].is_object()) {
            const auto& roof = data["roof"];
            const std::string roofName = prefix + roof.value("name", "roof");
            const bool triCol = (roof.value("collision", "box") == "mesh");
            const glm::mat4 roofModel = transform
                          * glm::translate(glm::mat4(1.0f),
                               readVec3(roof.value("center", json()), glm::vec3(0.0f)))
                          * rotationMatrix(readVec3(roof.value("rotation", json()), glm::vec3(0.0f)));
            const glm::vec2 span = readVec2(roof.value("span", json()), glm::vec2(1.0f));
            const float roofHeight = roof.value("height", 2.0f);
            const float roofThickness = roof.value("thickness", 0.15f);

            MapPart part;
            part.name     = roofName;
            part.material = readMaterial(roof.value("material", json()));
            part.triangleCollision = triCol;
            part.model    = roofModel;
            part.mesh     = buildRoofMesh(span, roofHeight, roofThickness);
            m_parts.push_back(std::move(part));

            MapPart gable;
            gable.name     = roofName + "_gables";
            gable.material = readMaterial(roof.value("gableMaterial", json()));
            gable.triangleCollision = triCol;
            gable.model    = roofModel;
            gable.mesh     = buildGablesMesh(span, roofHeight, roofThickness);
            m_parts.push_back(std::move(gable));
        }

        // 3. Écrans (TV) : placement + taille (l'easter egg DVD animé est
        //    créé/dessiné par la Scene — classe TvScreen — quand le joueur
        //    est assez proche). Pas de mesh, pas de collision.
        if (data.contains("tvs") && data["tvs"].is_array()) {
            for (const auto& tv : data["tvs"]) {
                MapScreen screen;
                screen.name   = prefix + tv.value("name", "tv");
                screen.width  = tv.value("width", 1.0f);
                screen.height = tv.value("height", 0.6f);
                screen.model  = transform
                              * glm::translate(glm::mat4(1.0f),
                                   readVec3(tv.value("center", json()), glm::vec3(0.0f)))
                              * rotationMatrix(readVec3(tv.value("rotation", json()), glm::vec3(0.0f)));
                m_screens.push_back(std::move(screen));
            }
        }

        // 4. Portes interactives (charnière transformée par l'include)
        if (data.contains("doors") && data["doors"].is_array()) {
            for (const auto& door : data["doors"]) {
                const glm::vec3 hinge = readVec3(door.value("hinge", json()), glm::vec3(0.0f));
                const glm::vec3 worldHinge = glm::vec3(transform * glm::vec4(hinge, 1.0f));
                m_doors.push_back(std::make_unique<Door>(
                    worldHinge,
                    door.value("yaw", 0.0f) + yawOffsetDeg,
                    door.value("width", Constants::Door::DEFAULT_WIDTH),
                    door.value("height", Constants::Door::DEFAULT_HEIGHT),
                    door.value("thickness", Constants::Door::DEFAULT_THICKNESS)));
            }
        }

        // 8. Arbres : mesh OBJ partagé par type, collision triangles (hitbox fidèle).
        if (data.contains("trees") && data["trees"].is_array()) {
            for (const auto& tr : data["trees"]) {
                const int type = tr.value("type", 0);
                auto it = m_treeMeshes.find(type);
                if (it == m_treeMeshes.end()) {
                    auto mesh = loadObjMesh("./res/models/trees/tree_" + std::to_string(type) + ".obj");
                    if (!mesh) {
                        LOG_WARN("[Map] Arbre type %d introuvable, ignore", type);
                        continue;
                    }
                    it = m_treeMeshes.emplace(type, std::move(mesh)).first;
                }
                const float s = tr.value("scale", 1.0f);
                MapPart part;
                part.name     = prefix + "tree" + std::to_string(type);
                part.material = MapMaterial::Tree;
                part.triangleCollision = true;
                part.model    = transform
                              * glm::translate(glm::mat4(1.0f),
                                   readVec3(tr.value("position", json()), glm::vec3(0.0f)))
                              * glm::rotate(glm::mat4(1.0f), glm::radians(tr.value("yaw", 0.0f)),
                                            glm::vec3(0.0f, 1.0f, 0.0f))
                              * glm::scale(glm::mat4(1.0f), glm::vec3(s));
                part.mesh     = it->second;
                m_parts.push_back(std::move(part));
            }
        }

        // 5. Pièces : AABB locale → AABB monde (8 coins transformés, valable
        //    pour les yaw multiples de 90°).
        if (data.contains("rooms") && data["rooms"].is_array()) {
            for (const auto& r : data["rooms"]) {
                const glm::vec3 lo = readVec3(r.value("min", json()), glm::vec3(0.0f));
                const glm::vec3 hi = readVec3(r.value("max", json()), glm::vec3(0.0f));
                MapRoom room;
                room.name = prefix + r.value("name", "room");
                room.min  = glm::vec3(FLT_MAX);
                room.max  = glm::vec3(-FLT_MAX);
                for (int i = 0; i < 8; ++i) {
                    const glm::vec3 corner(i & 1 ? hi.x : lo.x, i & 2 ? hi.y : lo.y, i & 4 ? hi.z : lo.z);
                    const glm::vec3 w = glm::vec3(transform * glm::vec4(corner, 1.0f));
                    room.min = glm::min(room.min, w);
                    room.max = glm::max(room.max, w);
                }
                m_rooms.push_back(std::move(room));
            }
        }

        // 6. Lumières ponctuelles
        if (data.contains("lights") && data["lights"].is_array()) {
            for (const auto& l : data["lights"]) {
                MapLight light;
                light.position = glm::vec3(transform * glm::vec4(
                    readVec3(l.value("position", json()), glm::vec3(0.0f)), 1.0f));
                const std::string room = l.value("room", "");
                if (!room.empty()) light.room = prefix + room;
                light.ambient     = readVec3(l.value("ambient", json()),     light.ambient);
                light.diffuse     = readVec3(l.value("diffuse", json()),     light.diffuse);
                light.specular    = readVec3(l.value("specular", json()),    light.specular);
                light.attenuation = readVec3(l.value("attenuation", json()), light.attenuation);
                light.color       = readVec3(l.value("color", json()),       light.diffuse);
                m_lights.push_back(std::move(light));
            }
        }

        // 7. Points d'apparition nommés
        if (data.contains("spawns") && data["spawns"].is_array()) {
            for (const auto& s : data["spawns"]) {
                MapSpawn spawn;
                spawn.name = prefix + s.value("name", "spawn");
                spawn.position = glm::vec3(transform * glm::vec4(
                    readVec3(s.value("position", json()), glm::vec3(0.0f)), 1.0f));
                m_spawns.push_back(std::move(spawn));
            }
        }
    } catch (const json::exception& e) {
        LOG_ERROR("[Map] JSON invalide %s : %s", path.string().c_str(), e.what());
        ok = false;
    }

    stack.pop_back();
    return ok;
}
