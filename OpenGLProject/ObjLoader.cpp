#include "ObjLoader.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

#include "Mesh.h"
#include "MapGeometry.h"

// Charge un OBJ simple (v/vt/vn, faces v/vt/vn triangulées) en Mesh.
// nullptr si le fichier est absent ou vide.
std::shared_ptr<Mesh> loadObjMesh(const std::string& path) {
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

// Matériau d'un .mtl (map_Kd = chemin relatif au .mtl, Kd couleur, d opacité).

static std::map<std::string, ObjMaterial> loadMtl(const std::string& mtlPath) {
    std::map<std::string, ObjMaterial> mats;
    std::ifstream in(mtlPath);
    if (!in.is_open()) return mats;
    const std::filesystem::path dir = std::filesystem::path(mtlPath).parent_path();
    ObjMaterial* cur = nullptr;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag == "newmtl") {
            std::string name;
            ss >> name;
            cur = &mats[name];
        } else if (!cur) {
            continue;
        } else if (tag == "Kd") {
            ss >> cur->kd.x >> cur->kd.y >> cur->kd.z;
        } else if (tag == "d") {
            ss >> cur->alpha;
        } else if (tag == "map_Kd") {
            std::string rel;
            std::getline(ss >> std::ws, rel);
            cur->texture = (dir / rel).generic_string();
        }
    }
    return mats;
}

// Parties d'un OBJ (une par couple objet/matériau) : normales plates, UV du fichier
// (sinon projetés en repère monde), soupe de triangles MONDE pour la collision.
// Tout est transformé par `m`.

std::vector<ObjPart> loadObjParts(const std::string& path, const glm::mat4& m) {
    std::vector<ObjPart> parts;
    std::ifstream in(path);
    if (!in.is_open()) return parts;

    std::vector<glm::vec3> pos;
    std::vector<glm::vec2> uvs;
    std::map<std::string, ObjMaterial> mats;
    std::map<std::string, size_t> partIndex;   // "objet|materiau" -> parts[i]
    std::string curObject = "model", curMat;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag == "v") {
            glm::vec3 p;
            ss >> p.x >> p.y >> p.z;
            pos.push_back(glm::vec3(m * glm::vec4(p, 1.0f)));
        } else if (tag == "vt") {
            glm::vec2 t;
            ss >> t.x >> t.y;
            uvs.push_back(t);
        } else if (tag == "mtllib") {
            std::string file;
            std::getline(ss >> std::ws, file);
            const std::string mtl = (std::filesystem::path(path).parent_path() / file).generic_string();
            const auto loaded = loadMtl(mtl);
            mats.insert(loaded.begin(), loaded.end());
        } else if (tag == "o" || tag == "g") {
            ss >> curObject;
        } else if (tag == "usemtl") {
            ss >> curMat;
        } else if (tag == "f") {
            struct FV { int v, t; };
            std::vector<FV> f;
            std::string tok;
            while (ss >> tok) {
                FV fv{ std::atoi(tok.c_str()), 0 };   // "12", "12/3", "12//4", "12/3/4"
                const size_t s1 = tok.find('/');
                if (s1 != std::string::npos) fv.t = std::atoi(tok.c_str() + s1 + 1);
                if (fv.v >= 1 && fv.v <= static_cast<int>(pos.size())) f.push_back(fv);
            }
            if (f.size() < 3) continue;
            const glm::vec3& p0 = pos[f[0].v - 1];
            const glm::vec3 n0 = glm::cross(pos[f[1].v - 1] - p0, pos[f[2].v - 1] - p0);
            if (glm::length(n0) < 1e-8f) continue;
            const glm::vec3 n = glm::normalize(n0);

            const std::string key = curObject + "|" + curMat;
            auto it = partIndex.find(key);
            if (it == partIndex.end()) {
                it = partIndex.emplace(key, parts.size()).first;
                parts.push_back(ObjPart());
                parts.back().name = curObject + (curMat.empty() ? "" : "_" + curMat);
                const auto mi = mats.find(curMat);
                if (mi != mats.end()) parts.back().mat = mi->second;
            }
            ObjPart& part = parts[it->second];

            const unsigned int base = static_cast<unsigned int>(part.verts.size());
            for (const FV& fv : f) {
                const glm::vec3& p = pos[fv.v - 1];
                glm::vec2 uv;
                if (fv.t >= 1 && fv.t <= static_cast<int>(uvs.size())) {
                    uv = uvs[fv.t - 1];
                } else {   // pas de vt : projection monde selon la normale dominante
                    const glm::vec3 a = glm::abs(n);
                    if (a.x >= a.y && a.x >= a.z) uv = glm::vec2(p.z, p.y);
                    else if (a.y >= a.z)          uv = glm::vec2(p.x, p.z);
                    else                          uv = glm::vec2(p.x, p.y);
                    uv *= kUvScale;
                }
                part.verts.push_back(Vertex(p.x, p.y, p.z, n.x, n.y, n.z, uv.x, uv.y));
            }
            for (size_t k = 1; k + 1 < f.size(); ++k) {
                part.idx.insert(part.idx.end(), { base, base + static_cast<unsigned int>(k), base + static_cast<unsigned int>(k + 1) });
                part.soup.push_back(p0);
                part.soup.push_back(pos[f[k].v - 1]);
                part.soup.push_back(pos[f[k + 1].v - 1]);
            }
        }
    }
    return parts;
}

