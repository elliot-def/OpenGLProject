#include "MapLoader.h"

#include <algorithm>
#include <cfloat>
#include <random>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <cstdio>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include <glm/gtc/matrix_transform.hpp>

#include "Mesh.h"
#include "MapGeometry.h"
#include "ObjLoader.h"
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

// Planches de feuilles : colonnes x lignes (cellules = une feuille chacune).
static const int kLeafCols[3] = { 3, 3, 2 };
static const int kLeafRows[3] = { 2, 2, 3 };

// Aleatoire portable (mt19937 est deterministe, pas les distributions std).
static float rnd01(std::mt19937& rng) { return (rng() >> 8) / 16777216.0f; }

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
    m_buttons.clear();
    m_hasPatrol = false;
    m_sun = MapSun();
    m_fog = MapFog();
    m_sunHead = MapSunHead();
    m_creatureEnabled = true;
    m_creatureSight = 0.0f;
    m_skybox.clear();
    m_treeCounter = 0;
    for (int i = 0; i < LEAF_SHEETS; ++i) {
        m_leafVerts[i].clear(); m_leafIdx[i].clear(); m_leafMeshes[i].reset();
    }

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

    for (int i = 0; i < LEAF_SHEETS; ++i) {
        if (m_leafIdx[i].empty()) continue;
        m_leafMeshes[i] = std::make_unique<Mesh>(m_leafVerts[i], m_leafIdx[i],
            (unsigned int)VertexAttribute::POSITION |
            (unsigned int)VertexAttribute::NORMAL |
            (unsigned int)VertexAttribute::TEXCOORD);
    }

    // Collisions statiques (les portes gèrent leurs colliders dynamiques
    // côté Scene) puis construction du BVH du niveau. "collision": "mesh" →
    // triangles (la hitbox suit la géométrie : pente du toit...), sinon AABB.
    for (auto& part : m_parts) {
        if (!part.soup.empty()) {
            m_collisionManager->addStaticTriangleSoup(part.soup, part.name);
            part.soup.clear();
            part.soup.shrink_to_fit();
        } else if (part.triangleCollision) {
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

        // -0.5. Brouillard (racine uniquement)
        if (depth == 0 && data.contains("fog") && data["fog"].is_object()) {
            const json& fog = data["fog"];
            m_fog.color   = readVec3(fog.value("color", json()), m_fog.color);
            m_fog.density = fog.value("density", m_fog.density);
        }

        // -0.4. Ciel, tete du soleil/lune, portee de vue de la creature (racine uniquement)
        if (depth == 0) {
            if (data.contains("skybox") && data["skybox"].is_string()) m_skybox = data["skybox"].get<std::string>();
            if (data.contains("sunHead") && data["sunHead"].is_object()) {
                const json& sh = data["sunHead"];
                m_sunHead.color          = readVec3(sh.value("color", json()), m_sunHead.color);
                m_sunHead.distance       = sh.value("distance", m_sunHead.distance);
                m_sunHead.scale          = sh.value("scale", m_sunHead.scale);
                m_sunHead.spotIntensity  = sh.value("spotIntensity", m_sunHead.spotIntensity);
            }
            if (data.contains("creature") && data["creature"].is_object()) {
                m_creatureSight = data["creature"].value("sightRange", 0.0f);
                m_creatureEnabled = data["creature"].value("enabled", true);
                if (data["creature"].contains("patrol") && data["creature"]["patrol"].is_object()) {
                    const json& pt = data["creature"]["patrol"];
                    m_patrolMin = readVec2(pt.value("min", json()), m_patrolMin);
                    m_patrolMax = readVec2(pt.value("max", json()), m_patrolMax);
                    m_hasPatrol = m_patrolMax.x > m_patrolMin.x && m_patrolMax.y > m_patrolMin.y;
                }
            }
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
            gable.mesh     = buildGablesMesh(span, roofHeight, roofThickness,
                                             roof.value("gableInset", 0.0f),
                                             roof.value("gableBase", -roofThickness),
                                             roof.value("gableThickness", 0.0f), roofModel);
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

        // 7.bis Modeles OBJ
        if (data.contains("models") && data["models"].is_array()) {
            for (const auto& md : data["models"]) {
                const std::string mpath = md.value("path", "");
                const float ms = md.value("scale", 1.0f);
                const float msy = md.value("scaleY", ms);
                const glm::mat4 mm = transform
                    * glm::translate(glm::mat4(1.0f), readVec3(md.value("position", json()), glm::vec3(0.0f)))
                    * glm::rotate(glm::mat4(1.0f), glm::radians(md.value("yaw", 0.0f)), glm::vec3(0.0f, 1.0f, 0.0f))
                    * glm::scale(glm::mat4(1.0f), glm::vec3(ms, msy, ms));
                std::vector<ObjPart> objParts = loadObjParts(mpath, mm);
                if (objParts.empty()) {
                    LOG_WARN("[Map] Modele OBJ introuvable ou vide : %s", mpath.c_str());
                    continue;
                }
                for (auto& op : objParts) {
                    if (op.idx.empty()) continue;
                    MapPart part;
                    part.name     = prefix + "model_" + op.name;
                    part.material = MapMaterial::Textured;
                    part.texturePath = op.mat.texture;
                    part.color    = op.mat.kd;
                    part.alpha    = op.mat.alpha;
                    part.model    = glm::mat4(1.0f);   // sommets deja en repere monde
                    part.soup     = std::move(op.soup);
                    part.mesh     = std::make_shared<Mesh>(op.verts, op.idx,
                        (unsigned int)VertexAttribute::POSITION |
                        (unsigned int)VertexAttribute::NORMAL |
                        (unsigned int)VertexAttribute::TEXCOORD);
                    m_parts.push_back(std::move(part));
                }
                LOG_INFO("[Map] Modele charge : %s (%zu objets)", mpath.c_str(), objParts.size());
            }
        }

        // 8. Arbres : mesh OBJ partagé par type, collision triangles (hitbox fidèle).
        if (data.contains("trees") && data["trees"].is_array()) {
            const json leaves = data.value("leaves", json::object());
            const int leafCount = leaves.value("perTree", 0);
            const float leafRadius = leaves.value("radius", 3.5f);
            const unsigned leafSeed = leaves.value("seed", 1337u);
            const glm::vec2 leafSize = readVec2(leaves.value("size", json()), glm::vec2(0.35f, 0.7f));
            const float leafSizeMin = leafSize.x, leafSizeMax = std::max(leafSize.x, leafSize.y);
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
                const float sy = tr.value("scaleY", s);
                MapPart part;
                part.name     = prefix + "tree" + std::to_string(type);
                part.material = MapMaterial::Tree;
                part.triangleCollision = true;
                part.model    = transform
                              * glm::translate(glm::mat4(1.0f),
                                   readVec3(tr.value("position", json()), glm::vec3(0.0f)))
                              * glm::rotate(glm::mat4(1.0f), glm::radians(tr.value("yaw", 0.0f)),
                                            glm::vec3(0.0f, 1.0f, 0.0f))
                              * glm::scale(glm::mat4(1.0f), glm::vec3(s, sy, s));
                part.mesh     = it->second;
                {   // delta de teinte propre a l'arbre (+-18 %), derive de la seed -> toujours le meme
                    std::mt19937 trng(leafSeed * 2654435761u ^ (m_treeCounter * 40503u + 1u));
                    const float d = (rnd01(trng) - 0.5f) * 0.36f;
                    part.tint = glm::vec3(1.0f + d * 0.9f, 1.0f + d, 1.0f + d * 0.8f);
                }
                const glm::vec3 treePos = glm::vec3(part.model * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
                m_parts.push_back(std::move(part));

                // Feuilles au sol : seed + index d'arbre -> toujours les memes positions.
                if (leafCount > 0) {
                    std::mt19937 rng(leafSeed + 7919u * m_treeCounter);
                    for (int i = 0; i < leafCount; ++i) {
                        const int sheet = (int)(rnd01(rng) * LEAF_SHEETS) % LEAF_SHEETS;
                        const int cols = kLeafCols[sheet], rows = kLeafRows[sheet];
                        const int cell = (int)(rnd01(rng) * cols * rows) % (cols * rows);
                        const float ang = rnd01(rng) * 6.2831853f;
                        const float r = 0.6f + leafRadius * s * std::pow(rnd01(rng), 0.7f);
                        const float yaw = rnd01(rng) * 6.2831853f;
                        const float h = leafSizeMin + (leafSizeMax - leafSizeMin) * rnd01(rng);
                        const float w = h * rows / cols;   // aspect de la cellule
                        const glm::vec3 c(treePos.x + r * std::cos(ang),
                                          0.02f + 0.0004f * (float)(m_leafIdx[sheet].size() / 6 % 100),   // evite le z-fighting entre feuilles
                                          treePos.z + r * std::sin(ang));
                        const float u0 = (cell % cols) / (float)cols + 0.02f, u1 = (cell % cols + 1) / (float)cols - 0.02f;
                        const float rowTop = (float)(cell / cols);
                        const float v0 = 1.0f - (rowTop + 1.0f) / rows + 0.02f, v1 = 1.0f - rowTop / rows - 0.02f;
                        const float cy = std::cos(yaw), sn = std::sin(yaw);
                        auto corner = [&](float lx, float lz, float u, float v) {
                            return Vertex(c.x + lx * cy - lz * sn, c.y, c.z + lx * sn + lz * cy,
                                          0.0f, 1.0f, 0.0f, u, v);
                        };
                        auto& vs = m_leafVerts[sheet];
                        const unsigned int base = (unsigned int)vs.size();
                        vs.push_back(corner(-w * 0.5f, -h * 0.5f, u0, v0));
                        vs.push_back(corner( w * 0.5f, -h * 0.5f, u1, v0));
                        vs.push_back(corner( w * 0.5f,  h * 0.5f, u1, v1));
                        vs.push_back(corner(-w * 0.5f,  h * 0.5f, u0, v1));
                        m_leafIdx[sheet].insert(m_leafIdx[sheet].end(),
                            { base, base + 1, base + 2, base + 2, base + 3, base });
                    }
                }
                ++m_treeCounter;
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

        // 6.bis Boutons interactifs (pave + collision boite)
        if (data.contains("buttons") && data["buttons"].is_array()) {
            for (const auto& bt : data["buttons"]) {
                const glm::vec3 center = readVec3(bt.value("center", json()), glm::vec3(0.0f));
                MapButton button;
                button.name = bt.value("name", "button");
                button.label = bt.value("label", button.name);
                button.scale = bt.value("scale", 0.45f);
                button.position = glm::vec3(transform * glm::vec4(center, 1.0f));
                m_buttons.push_back(std::move(button));
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
