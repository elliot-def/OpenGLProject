#include "MapLoader.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

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
    for (auto& v : vertices) {
        const glm::vec3 world = glm::vec3(model * glm::vec4(v.getPositions(), 1.0f));
        const glm::vec3 n = glm::abs(v.getNormals());
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
// prisme plein. Le pignon est désormais porté par buildGablesMesh (base en
// y=-thickness pour fermer le bord des dalles).
//
// Par dalle (côté s, s=-1 ou +1, hauteur h) :            (extrémités z ouvertes
//   dessus  (s·hx, 0) → (0, h)                           ≠ z=±hz : les pignons
//   dessous (s·hx, -t) → (0, h-t)  (décalage vertical)   les referment)
//   tranche d'égout verticale à x=s·hx entre y=0 et y=-t
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
    vertices.reserve(21);   // 2 dalles × (8 sommets dessus/dessous + 4 égout)
    indices.reserve(36);

    // Dalle -X
    pushQuad(vertices, indices, A, D, R1, R0, span.y, slopeLen);       // dessus
    pushQuad(vertices, indices, A2, R02, R12, D2, span.y, slopeLen);   // dessous
    pushQuad(vertices, indices, A, A2, D2, D, t, span.y);              // égout (-X)

    // Dalle +X
    pushQuad(vertices, indices, R1, C, B, R0, span.y, slopeLen);       // dessus
    pushQuad(vertices, indices, R02, B2, C2, R12, span.y, slopeLen);   // dessous
    pushQuad(vertices, indices, C, C2, B2, B, t, span.y);              // égout (+X)

    return std::make_unique<Mesh>(vertices, indices,
        (unsigned int)VertexAttribute::POSITION |
        (unsigned int)VertexAttribute::NORMAL |
        (unsigned int)VertexAttribute::TEXCOORD);
}

// Pignons : 2 triangles verticaux aux extrémités du faîtage (la façade
// continue les murs jusqu'au toit). Base en y=-thickness pour couvrir aussi
// le bord ouvert des dalles de toit (arête d'égout) : fermeture complète,
// même plan que le dessus des dalles mais aucune face coplanaire (les dalles
// n'ont pas de face aux extrémités z).
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
    const glm::vec3 R0(0.0f, height, -hz);   // extrémités du faîtage
    const glm::vec3 R1(0.0f, height,  hz);

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    vertices.reserve(6);
    indices.reserve(6);

    pushTriangle(vertices, indices,
                 D, {0.0f, 0.0f}, C, {span.x, 0.0f},
                 R1, {hx, height + thickness});   // +Z
    pushTriangle(vertices, indices,
                 B, {span.x, 0.0f}, A, {0.0f, 0.0f},
                 R0, {hx, height + thickness});   // -Z

    return std::make_unique<Mesh>(vertices, indices,
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
    m_doors.clear();

    std::vector<std::string> stack;
    if (!loadMapFile(jsonPath, glm::mat4(1.0f), 0.0f, 0, "", stack)) {
        m_parts.clear();
        m_doors.clear();
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

    LOG_INFO("[Map] Carte chargee : %s (%zu parties, %zu portes)",
             jsonPath.c_str(), m_parts.size(), m_doors.size());
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

        // 3. Portes interactives (charnière transformée par l'include)
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
    } catch (const json::exception& e) {
        LOG_ERROR("[Map] JSON invalide %s : %s", path.string().c_str(), e.what());
        ok = false;
    }

    stack.pop_back();
    return ok;
}
