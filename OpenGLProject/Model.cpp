#include "Model.h"
#include "Mesh.h"
#include "Shader.h"
#include "TextureManager.h"
#include "Vertex.h"
#include "Camera.h"
#include "LightManager.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <utility> // std::declval pour le static_assert(noexcept(Mesh::~Mesh)).
#include <vector>

#include <glad/glad.h>
#include <nlohmann/json.hpp>
#include <stb/stb_image.h>

namespace {
using json = nlohmann::json;

struct GlbAnimationData {
    json document;
    std::vector<std::uint8_t> binary;
};

bool readGlb(const std::string& path, GlbAnimationData& result) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;

    const std::vector<std::uint8_t> bytes(
        (std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (bytes.size() < 12) return false;

    const auto readU32 = [&bytes](std::size_t offset) -> std::uint32_t {
        return static_cast<std::uint32_t>(bytes[offset]) |
               (static_cast<std::uint32_t>(bytes[offset + 1]) << 8u) |
               (static_cast<std::uint32_t>(bytes[offset + 2]) << 16u) |
               (static_cast<std::uint32_t>(bytes[offset + 3]) << 24u);
    };

    if (readU32(0) != 0x46546C67u || readU32(4) != 2u) return false;

    bool gotJson = false;
    std::size_t offset = 12;
    while (offset + 8 <= bytes.size()) {
        const std::uint32_t chunkLength = readU32(offset);
        const std::uint32_t chunkType = readU32(offset + 4);
        offset += 8;
        if (chunkLength > bytes.size() - offset) return false;

        if (chunkType == 0x4E4F534Au) { // JSON
            const std::string text(reinterpret_cast<const char*>(bytes.data() + offset), chunkLength);
            result.document = json::parse(text, nullptr, false);
            if (result.document.is_discarded()) return false;
            gotJson = true;
        } else if (chunkType == 0x004E4942u) { // BIN\0
            result.binary.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                                 bytes.begin() + static_cast<std::ptrdiff_t>(offset + chunkLength));
        }
        offset += chunkLength;
    }

    return gotJson && !result.binary.empty();
}

bool readFloatAccessor(const json& document, const std::vector<std::uint8_t>& binary,
                       int accessorIndex, unsigned int components, std::vector<float>& values) {
    if (!document.contains("accessors") || !document["accessors"].is_array() ||
        accessorIndex < 0 || accessorIndex >= static_cast<int>(document["accessors"].size())) {
        return false;
    }

    const json& accessor = document["accessors"][accessorIndex];
    if (!accessor.contains("bufferView") || !accessor.contains("count") ||
        accessor.value("componentType", 0) != 5126) { // FLOAT
        return false;
    }

    const std::string type = accessor.value("type", "");
    if ((components == 1 && type != "SCALAR") ||
        (components == 4 && type != "VEC4")) {
        return false;
    }

    const int viewIndex = accessor.value("bufferView", -1);
    if (!document.contains("bufferViews") || !document["bufferViews"].is_array() ||
        viewIndex < 0 || viewIndex >= static_cast<int>(document["bufferViews"].size())) {
        return false;
    }

    const json& view = document["bufferViews"][viewIndex];
    const std::size_t count = accessor.value("count", 0u);
    const std::size_t accessorOffset = accessor.value("byteOffset", 0u);
    const std::size_t viewOffset = view.value("byteOffset", 0u);
    const std::size_t elementSize = static_cast<std::size_t>(components) * sizeof(float);
    const std::size_t stride = view.value("byteStride", elementSize);
    if (stride < elementSize || count == 0) return false;

    if (viewOffset > binary.size() || accessorOffset > binary.size() - viewOffset) return false;
    const std::size_t start = viewOffset + accessorOffset;
    if (start > binary.size() || elementSize > binary.size() - start) return false;
    if (count > 1 && (count - 1) > (binary.size() - start - elementSize) / stride) {
        return false;
    }
    const std::size_t end = start + (count - 1) * stride + elementSize;
    if (end > binary.size()) return false;

    if (count > std::numeric_limits<std::size_t>::max() / components) return false;
    values.resize(count * components);
    for (std::size_t i = 0; i < count; ++i) {
        for (unsigned int component = 0; component < components; ++component) {
            const std::size_t byteOffset = start + i * stride + component * sizeof(float);
            std::memcpy(&values[i * components + component], binary.data() + byteOffset, sizeof(float));
        }
    }
    return true;
}

bool patchGlbAnimationRotations(const aiScene* scene, const std::string& path) {
    if (!scene || scene->mNumAnimations == 0) return false;

    std::string extension = std::filesystem::path(path).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (extension != ".glb") return false;

    GlbAnimationData glb;
    if (!readGlb(path, glb) || !glb.document.contains("animations") ||
        !glb.document["animations"].is_array()) {
        std::cerr << "[Model] Impossible de lire les animations brutes du GLB: " << path << std::endl;
        return false;
    }

    unsigned int restoredChannels = 0;
    unsigned int restoredKeys = 0;
    const json& sourceAnimations = glb.document["animations"];

    for (std::size_t sourceIndex = 0; sourceIndex < sourceAnimations.size(); ++sourceIndex) {
        const json& sourceAnimation = sourceAnimations[sourceIndex];
        const std::string sourceName = sourceAnimation.value("name", "");
        const aiAnimation* targetAnimation = nullptr;

        // Les index sont normalement identiques, mais le nom évite de patcher
        // la mauvaise animation si un importer réordonne les animations.
        for (unsigned int i = 0; i < scene->mNumAnimations; ++i) {
            if (!sourceName.empty() && sourceName == scene->mAnimations[i]->mName.C_Str()) {
                targetAnimation = scene->mAnimations[i];
                break;
            }
        }
        if (!targetAnimation && sourceIndex < scene->mNumAnimations) {
            targetAnimation = scene->mAnimations[sourceIndex];
        }
        if (!targetAnimation || !sourceAnimation.contains("channels") ||
            !sourceAnimation["channels"].is_array() ||
            !sourceAnimation.contains("samplers") || !sourceAnimation["samplers"].is_array()) {
            continue;
        }

        std::unordered_map<std::string, aiNodeAnim*> channels;
        for (unsigned int i = 0; i < targetAnimation->mNumChannels; ++i) {
            channels[targetAnimation->mChannels[i]->mNodeName.C_Str()] = targetAnimation->mChannels[i];
        }

        for (const json& sourceChannel : sourceAnimation["channels"]) {
            if (!sourceChannel.contains("target") ||
                sourceChannel["target"].value("path", "") != "rotation") {
                continue;
            }
            const int nodeIndex = sourceChannel["target"].value("node", -1);
            const int samplerIndex = sourceChannel.value("sampler", -1);
            if (nodeIndex < 0 || !glb.document.contains("nodes") ||
                nodeIndex >= static_cast<int>(glb.document["nodes"].size()) ||
                samplerIndex < 0 || samplerIndex >= static_cast<int>(sourceAnimation["samplers"].size())) {
                continue;
            }

            const std::string nodeName = glb.document["nodes"][nodeIndex].value("name", "");
            auto channelIt = channels.find(nodeName);
            if (nodeName.empty() || channelIt == channels.end()) continue;

            const json& sampler = sourceAnimation["samplers"][samplerIndex];
            const int inputAccessor = sampler.value("input", -1);
            const int outputAccessor = sampler.value("output", -1);
            std::vector<float> times;
            std::vector<float> rotations;
            if (!readFloatAccessor(glb.document, glb.binary, inputAccessor, 1, times) ||
                !readFloatAccessor(glb.document, glb.binary, outputAccessor, 4, rotations) ||
                times.size() * 4 != rotations.size()) {
                continue;
            }

            float ticksPerSecond = static_cast<float>(targetAnimation->mTicksPerSecond);
            if (!std::isfinite(ticksPerSecond) || ticksPerSecond <= 0.001f) ticksPerSecond = 1.0f;

            std::vector<aiQuatKey> repairedKeys;
            repairedKeys.reserve(times.size());
            bool valid = true;
            for (std::size_t key = 0; key < times.size(); ++key) {
                const float x = rotations[key * 4 + 0];
                const float y = rotations[key * 4 + 1];
                const float z = rotations[key * 4 + 2];
                const float w = rotations[key * 4 + 3];
                const float length = std::sqrt(x * x + y * y + z * z + w * w);
                if (!std::isfinite(times[key]) || !std::isfinite(length) || length <= 0.0001f) {
                    valid = false;
                    break;
                }
                repairedKeys.emplace_back(static_cast<double>(times[key]) * ticksPerSecond,
                                          aiQuaternion(w / length, x / length, y / length, z / length));
            }
            if (!valid || repairedKeys.empty()) continue;

            aiNodeAnim* targetChannel = channelIt->second;
            if (!targetChannel) {
                std::cerr << "[Model] Canal d'animation null pour le noeud \"" << nodeName << "\" — ignore" << std::endl;
                continue;
            }
            // In-place overwrite : on ne delete[] JAMAIS la memoire allouee
            // par Assimp (CRT different → heap corruption). On ecrase les
            // cles corrompues dans le buffer existant et on reduit le count.
            // repairedKeys.size() <= times.size() == targetChannel->mNumRotationKeys
            // (readFloatAccessor lit le meme accessor qu'Assimp) donc pas de
            // depassement.
            unsigned int newSize = std::min(targetChannel->mNumRotationKeys,
                                            static_cast<unsigned int>(repairedKeys.size()));
            std::copy_n(repairedKeys.begin(), newSize, targetChannel->mRotationKeys);
            targetChannel->mNumRotationKeys = newSize;
            restoredChannels++;
            restoredKeys += newSize;
        }
    }

    if (restoredChannels > 0) {
        std::cout << "[Model] GLB rotations restaurees: " << restoredChannels
                  << " channels, " << restoredKeys << " cles" << std::endl;
    }
    return restoredChannels > 0;
}
}

// ===== BoundingBox =====
BoundingBox BoundingBox::transform(const glm::mat4& matrix) const {
    BoundingBox result;

    // Les 8 coins de la box
    glm::vec3 corners[8] = {
        glm::vec3(min.x, min.y, min.z),
        glm::vec3(max.x, min.y, min.z),
        glm::vec3(min.x, max.y, min.z),
        glm::vec3(max.x, max.y, min.z),
        glm::vec3(min.x, min.y, max.z),
        glm::vec3(max.x, min.y, max.z),
        glm::vec3(min.x, max.y, max.z),
        glm::vec3(max.x, max.y, max.z)
    };

    // Transformer chaque coin et recalculer min/max
    for (int i = 0; i < 8; i++) {
        glm::vec4 transformed = matrix * glm::vec4(corners[i], 1.0f);
        result.expand(glm::vec3(transformed));
    }

    return result;
}

bool BoundingBox::intersects(const BoundingBox& other) const {
    return (min.x <= other.max.x && max.x >= other.min.x) &&
        (min.y <= other.max.y && max.y >= other.min.y) &&
        (min.z <= other.max.z && max.z >= other.min.z);
}

// ===== BoundingSphere =====
BoundingSphere::BoundingSphere(const BoundingBox& box) {
    center = box.getCenter();
    radius = glm::length(box.getSize()) * 0.5f;
}

BoundingSphere BoundingSphere::transform(const glm::mat4& matrix) const {
    BoundingSphere result;
    glm::vec4 transformedCenter = matrix * glm::vec4(center, 1.0f);
    result.center = glm::vec3(transformedCenter);

    // Pour le rayon, on prend l'�chelle maximale
    glm::vec3 scale;
    scale.x = glm::length(glm::vec3(matrix[0]));
    scale.y = glm::length(glm::vec3(matrix[1]));
    scale.z = glm::length(glm::vec3(matrix[2]));
    result.radius = radius * glm::max(glm::max(scale.x, scale.y), scale.z);

    return result;
}

bool BoundingSphere::intersects(const BoundingSphere& other) const {
    float distance = glm::length(center - other.center);
    return distance < (radius + other.radius);
}

// ===== Model =====
Model::Model(Camera* camera, LightManager* lightManager, const std::string& path, TextureManager* textureManager)
    : m_sourcePath(path), m_camera(camera), m_lightManager(lightManager), m_textureManager(textureManager), m_debugBoundingBoxMesh(nullptr) {
    loadModel(path);
    calculateBoundingBox();
    calculateBoundingSphere();
    createDebugBoundingBoxMesh();
}

Model::~Model() {
    // Compile-time guard : Model::~Model() ne wrappe pas `delete` dans
    // try/catch (le guideline maison "pas de try/catch inutile" prime).
    // Ce destructeur repose donc sur le fait que Mesh::~Mesh reste noexcept.
    // Si un contributeur futur y ajoute un throw (ou un membre qui throw),
    // la compilation casse ICI -- la reponse adequate est alors soit de
    // rendre le membre noexcept, soit de migrer m_meshes vers
    // std::vector<std::unique_ptr<Mesh>>.
    static_assert(noexcept(std::declval<Mesh&>().~Mesh()),
                  "Mesh::~Mesh doit rester noexcept -- Model::~Model ne wrappe pas delete en try/catch.");
    // Libere tous les Mesh* alloues via new : sinon fuite memoire complete
    // de la hierarchie modele (chaque processMesh + le debug bounding box mesh
    // restent sur le tas jusqu'a la fin du process). Les Mesh* que nous
    // avons convertis en C++ doivent etre deletes ici.
    for (Mesh* mesh : m_meshes) {
        delete mesh;
    }
    m_meshes.clear();
    delete m_debugBoundingBoxMesh;
    m_debugBoundingBoxMesh = nullptr;
}

void Model::draw(Shader& shader) {
    shader.use();
    shader.setInt("texture_diffuse", 0);
    shader.setInt("texture_specular", 1);
    shader.setVec3("viewPos", m_camera->getPosition());
    m_lightManager->applyToShader(&shader);

    shader.setInt("texture_diffuse", 0);
    shader.setInt("texture_specular", 1);

    for (auto* mesh : m_meshes) {
        mesh->draw();
    }
}

void Model::drawBoundingBox(Shader& shader) {
    if (m_debugBoundingBoxMesh) {
        shader.use();
        shader.setVec3("color", glm::vec3(0.0f, 1.0f, 0.0f)); // Vert
        m_debugBoundingBoxMesh->draw();
    }
}

BoundingBox Model::getTransformedBoundingBox(const glm::mat4& modelMatrix) const {
    return m_boundingBox.transform(modelMatrix);
}

BoundingSphere Model::getTransformedBoundingSphere(const glm::mat4& modelMatrix) const {
    return m_boundingSphere.transform(modelMatrix);
}

bool Model::checkCollision(const Model& other, const glm::mat4& thisMatrix,
    const glm::mat4& otherMatrix) const {
    // Test rapide avec les sph�res
    BoundingSphere thisSphere = getTransformedBoundingSphere(thisMatrix);
    BoundingSphere otherSphere = other.getTransformedBoundingSphere(otherMatrix);

    if (!thisSphere.intersects(otherSphere)) {
        return false; // Pas de collision
    }

    // Test pr�cis avec les boxes
    BoundingBox thisBox = getTransformedBoundingBox(thisMatrix);
    BoundingBox otherBox = other.getTransformedBoundingBox(otherMatrix);

    return thisBox.intersects(otherBox);
}

bool Model::raycast(const glm::vec3& origin, const glm::vec3& direction,
    const glm::mat4& modelMatrix, float& distance) const {
    // Transformer le rayon dans l'espace du mod�le
    glm::mat4 invMatrix = glm::inverse(modelMatrix);
    glm::vec3 localOrigin = glm::vec3(invMatrix * glm::vec4(origin, 1.0f));
    glm::vec3 localDir = glm::normalize(glm::vec3(invMatrix * glm::vec4(direction, 0.0f)));

    // Test d'intersection rayon-box (algorithme slab)
    glm::vec3 tMin = (m_boundingBox.min - localOrigin) / localDir;
    glm::vec3 tMax = (m_boundingBox.max - localOrigin) / localDir;

    glm::vec3 t1 = glm::min(tMin, tMax);
    glm::vec3 t2 = glm::max(tMin, tMax);

    float tNear = glm::max(glm::max(t1.x, t1.y), t1.z);
    float tFar = glm::min(glm::min(t2.x, t2.y), t2.z);

    if (tNear > tFar || tFar < 0) {
        return false;
    }

    distance = tNear > 0 ? tNear : tFar;
    return true;
}

void Model::loadModel(const std::string& path) {
    m_processedMeshIndices.clear();
    m_boneInfoMap.clear();
    m_boneCounter = 0;

    m_scene = m_importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals
    );

    if (!m_scene || m_scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !m_scene->mRootNode) {
        std::cerr << "ERREUR::ASSIMP::" << m_importer.GetErrorString() << std::endl;
        return;
    }

    std::filesystem::path p = path;
    m_directory = p.parent_path().string();

    // Assimp 5.4.x fournit des aiQuatKey corrompues pour ce GLB, alors que
    // les accessors glTF sont corrects. Restaurer les rotations brutes avant
    // que Animator ne construise son cache ou n'interpole les canaux.
    patchGlbAnimationRotations(m_scene, path);

    processNode(m_scene->mRootNode, m_scene);

    // Identifier les noeuds joints du squelette conserve (apres processNode,
    // qui a deja ecarte les meshes des autres squelettes). L'Animator s'en
    // sert pour ne skinner que ce squelette.
    buildJointNodes();
}

// ---------------------------------------------------------------------------
// Squelette conserve : noeuds joints par nom (premier de l'ordre du fichier)
// ---------------------------------------------------------------------------

void Model::buildJointNodes() {
    m_jointNodes.clear();
    if (!m_scene || !m_scene->mRootNode) return;

    std::unordered_set<std::string> claimed;
    collectJointNodes(m_scene->mRootNode, claimed);

    if (m_boneCounter > 0 && m_jointNodes.empty()) {
        std::cerr << "[Model] Aucun noeud joint identifie (" << m_boneCounter
                  << " bones) : l'Animator retombera sur la recherche par nom." << std::endl;
    }
}

void Model::collectJointNodes(const aiNode* node, std::unordered_set<std::string>& claimed) {
    if (!node) return;

    // Reclamer le premier noeud de chaque nom de bone enregistre : le parcours
    // depth-first suit le meme ordre que l'enregistrement des bones dans
    // processMesh, donc c'est bien le squelette du mesh conserve qui gagne.
    // Les noeuds homonymes des autres rigs (ex: COG de FemaleArm) sont ignores.
    const std::string nodeName(node->mName.C_Str());
    if (m_boneInfoMap.find(nodeName) != m_boneInfoMap.end() && claimed.insert(nodeName).second) {
        m_jointNodes.insert(node);
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        collectJointNodes(node->mChildren[i], claimed);
    }
}

void Model::processNode(aiNode* node, const aiScene* scene) {
    // Traiter tous les meshes du noeud
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        unsigned int meshIndex = node->mMeshes[i];
        // D�duplication : certains exports GLB/GLTF r�f�rencent le m�me mesh
        // depuis plusieurs nœuds (fr�quent avec Mixamo/Blender). Sans ce skip,
        // deux Mesh* identiques sont dessin�s � la m�me position → z-fighting.
        if (m_processedMeshIndices.count(meshIndex)) {
            std::cout << "[Model] Mesh duplicate ignor� (index=" << meshIndex
                      << ", node=\"" << node->mName.C_Str() << "\")" << std::endl;
            continue;
        }
        aiMesh* mesh = scene->mMeshes[meshIndex];
        Mesh* processed = processMesh(mesh, scene);
        // processMesh() peut retourner nullptr : mesh d'un AUTRE squelette
        // (même noms de bones mais offsets différents) → à ignorer.
        if (processed) {
            m_processedMeshIndices.insert(meshIndex);
            m_meshes.push_back(processed);
        }
    }

    // Traiter r�cursivement les enfants
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode(node->mChildren[i], scene);
    }
}

Mesh* Model::processMesh(aiMesh* mesh, const aiScene* scene) {
    // Garde anti-doublon de squelette : un mesh qui référence des bones déjà
    // enregistrés (même nom) mais avec une matrice d'offset DIFFÉRENTE
    // appartient à un AUTRE squelette. Ex: human_1.glb contient MaleArm et
    // FemaleArm, deux rigs dont les noms de bones sont identiques (COG, Hip,
    // Thigh.Right...). Comme les bone IDs sont attribués PAR NOM dans
    // m_boneInfoMap, garder les deux meshes → le second personnage est skiné
    // avec les matrices du premier et les deux se dessinent superposés à la
    // même position (décalage en hauteur). On ne conserve que le premier
    // squelette rencontré (ordre du fichier).
    for (unsigned int b = 0; b < mesh->mNumBones; b++) {
        const aiBone* bone = mesh->mBones[b];
        const std::string boneName(bone->mName.C_Str());
        auto it = m_boneInfoMap.find(boneName);
        if (it == m_boneInfoMap.end()) continue; // nouveau bone : pas un conflit

        const glm::mat4 incoming = aiMatrixToGlm(bone->mOffsetMatrix);
        const glm::mat4& existing = it->second.offsetMatrix;
        bool same = true;
        for (int c = 0; c < 4 && same; c++) {
            for (int r = 0; r < 4; r++) {
                if (std::fabs(existing[c][r] - incoming[c][r]) > 1e-4f) {
                    same = false;
                    break;
                }
            }
        }
        if (!same) {
            std::cout << "[Model] Mesh ignore (squelette different) : \""
                      << mesh->mName.C_Str() << "\" (bone \"" << boneName << "\")" << std::endl;
            return nullptr;
        }
    }

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<unsigned int> textureIDs;

    // Vertices
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        // Position

        glm::vec3 position, normal = glm::vec3(0.0f, 0.0f, 0.0f);
        glm::vec2 texCoords = glm::vec2(0.0f, 0.0f);

        position = glm::vec3(
            mesh->mVertices[i].x,
            mesh->mVertices[i].y,
            mesh->mVertices[i].z
        );

        // Normales
        if (mesh->HasNormals()) {
            normal = glm::vec3(
                mesh->mNormals[i].x,
                mesh->mNormals[i].y,
                mesh->mNormals[i].z
            );
        }

        // Coordonn�es de texture
        if (mesh->mTextureCoords[0]) {
            texCoords = glm::vec2(
                mesh->mTextureCoords[0][i].x,
                mesh->mTextureCoords[0][i].y
            );
        }
        else {
            texCoords = glm::vec2(0.0f, 0.0f);
        }

        Vertex vertex = Vertex(position.x, position.y, position.z, normal.x, normal.y, normal.z, texCoords.x, texCoords.y);
        vertices.push_back(vertex);
    }

    // Extraire les données de bones (offset matrices + poids par vertex)
    extractBoneDataFromMesh(mesh, vertices);

    // Indices
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }

    // Textures
    if (mesh->mMaterialIndex >= 0) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        auto diffuse = loadMaterialTextures(material, aiTextureType_DIFFUSE, scene);
        // glTF2/GLB : Assimp >= 5.2 mappe baseColorTexture sur
        // aiTextureType_BASE_COLOR (=12) et NON sur DIFFUSE. Sans ce fallback,
        // le slot diffuse reste vide -> seule la texture NOIRE du specular est
        // poussee -> le modele est rendu tout NOIR (bras, personnages rigges).
        if (diffuse.empty()) {
            diffuse = loadMaterialTextures(material, aiTextureType_BASE_COLOR, scene);
        }
        auto specular = loadMaterialTextures(material, aiTextureType_SPECULAR, scene);

        // 1. Diffuse : texture du modele, sinon fallback BLANC 1x1 (jamais noir)
        if (!diffuse.empty()) {
            textureIDs.push_back(diffuse[0]);
        }
        else {
            unsigned int whiteTextureID;
            glGenTextures(1, &whiteTextureID);
            glBindTexture(GL_TEXTURE_2D, whiteTextureID);
            unsigned char whitePixel[] = { 255, 255, 255, 255 };
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            textureIDs.push_back(whiteTextureID);
        }

        // 2. CORRECTION : Si aucune sp�culaire n'est trouv�e, on g�n�re/affecte un ID de texture noire
        if (specular.empty()) {
            unsigned int blackTextureID;
            glGenTextures(1, &blackTextureID);
            glBindTexture(GL_TEXTURE_2D, blackTextureID);
            unsigned char blackPixel[] = { 10, 10, 10, 255 }; // Gris tr�s sombre (faible sp�cularit� par d�faut)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, blackPixel);

            textureIDs.push_back(blackTextureID);
        }
        else {
            textureIDs.push_back(specular[0]);
        }
    }

    // Binder la texture diffuse avant de cr�er le mesh (si ton Mesh::draw() utilise l'unit� 0)
    if (!textureIDs.empty()) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureIDs[0]);
    }
    unsigned int mask = (unsigned int)VertexAttribute::POSITION |
        (unsigned int)VertexAttribute::NORMAL |
        (unsigned int)VertexAttribute::TEXCOORD;
    // Ajouter SKINNING si le mesh a des bones (évite d'activer les attributs
    // boneIDs/weights pour les meshes statiques → économise bande passante GPU)
    if (mesh->mNumBones > 0)
        mask |= (unsigned int)VertexAttribute::SKINNING;

    return new Mesh(vertices, indices, mask, textureIDs);
}

void Model::extractBoneDataFromMesh(aiMesh* mesh, std::vector<Vertex>& vertices) {
    for (unsigned int b = 0; b < mesh->mNumBones; b++) {
        aiBone* bone = mesh->mBones[b];
        std::string boneName(bone->mName.C_Str());

        int boneId = -1;
        auto it = m_boneInfoMap.find(boneName);
        if (it == m_boneInfoMap.end()) {
            boneId = m_boneCounter++;
            BoneInfo info;
            info.id = boneId;
            info.offsetMatrix = aiMatrixToGlm(bone->mOffsetMatrix);
            m_boneInfoMap[boneName] = info;
        } else {
            boneId = it->second.id;
        }

        if (boneId >= MAX_BONES) continue;

        for (unsigned int w = 0; w < bone->mNumWeights; w++) {
            unsigned int vertexId = bone->mWeights[w].mVertexId;
            float weight = bone->mWeights[w].mWeight;

            if (vertexId >= vertices.size()) continue;

            Vertex& v = vertices[vertexId];
            for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
                if (v.m_weights[i] == 0.0f) {
                    v.m_boneIDs[i] = boneId;
                    v.m_weights[i] = weight;
                    break;
                }
            }
        }
    }
}

std::vector<unsigned int> Model::loadMaterialTextures(aiMaterial* mat, aiTextureType type, const aiScene* scene) {
    std::vector<unsigned int> textureIDs;

    for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
        aiString str;
        mat->GetTexture(type, i, &str);
        std::string texPath(str.C_Str());

        // GLB/GLTF : textures embarquees referencees par des chemins "*0",
        // "*1" (index dans scene->mTextures[]). On les resout via
        // GetEmbeddedTexture() puis loadEmbeddedTexture() qui decode le
        // binaire compresse (PNG/JPEG) via stbi_load_from_memory. Sans cette
        // branche, le code precedent construisait "./res/rigging/arm/*0" ->
        // stbi_load echouait -> texture magenta de fallback {255,0,255} ->
        // clignotement rose/rouge sous la flashlight.
        if (!texPath.empty() && texPath[0] == '*') {
            const aiTexture* emb = scene ? scene->GetEmbeddedTexture(texPath.c_str()) : nullptr;
            if (emb) {
                textureIDs.push_back(loadEmbeddedTexture(emb, texPath));
            }
            else {
                std::cerr << "[Model] Texture embarquee introuvable : " << texPath << std::endl;
                // Fallback : on laisse le caller (processMesh) generer une
                // texture noire pour le slot specular, mais pour le diffuse on
                // ne pousse rien -> le caller detectera diffuse.empty() et
                // n'ajoutera pas de slot (Mesh::draw() gere le cas < 2 textures).
            }
            continue;
        }

        // FBX exporté depuis un autre poste : Assimp donne parfois des chemins
        // ABSOLUS de la machine d'origine (ex: "G:\Models\ps2\hand rig\...\arms_01.png").
        // Ces chemins n'existent pas ici -> stbi_load échoue -> texture magenta de debug.
        // On ne garde que le nom de fichier, résolu depuis le dossier du modèle.
        std::filesystem::path texFsPath(texPath);
        const bool isAbsolute = texFsPath.is_absolute() ||
                                (texPath.size() > 1 && texPath[1] == ':'); // lecteur Windows "G:\..."
        if (isAbsolute) {
            texPath = texFsPath.filename().string();
        }

        std::string fullPath = m_directory + "/" + texPath;
        textureIDs.push_back(loadTextureFromFile(fullPath));
    }

    return textureIDs;
}

unsigned int Model::loadEmbeddedTexture(const aiTexture* texture, const std::string& cacheKey) {
    // Cache partage avec loadTextureFromFile : la cle est le chemin "*N".
    auto it = m_loadedTextures.find(cacheKey);
    if (it != m_loadedTextures.end()) return it->second;

    unsigned int textureID = 0;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    int width = 0, height = 0, nrChannels = 0;
    unsigned char* data = nullptr;
    bool ownsData = false; // vrai si on doit stbi_image_free(data)

    if (texture->mHeight == 0) {
        // Texture compressee (PNG/JPEG) : pcData pointe vers les octets
        // compresses, mWidth = taille en octets. On decode via stb_image.
        // stbi_set_flip_vertically_on_load est un ETAT GLOBAL : on le reset a
        // false ici (comme loadTextureFromFile) car aiProcess_FlipUVs flippe
        // deja les UV, pas les pixels. Sans ce reset, si un autre chemin a mis
        // le flag a true, la texture embarquee serait fligee a l'envers.
        stbi_set_flip_vertically_on_load(false);
        data = stbi_load_from_memory(
            reinterpret_cast<const stbi_uc*>(texture->pcData),
            static_cast<int>(texture->mWidth),
            &width, &height, &nrChannels, 0);
        ownsData = true;
    }
    else {
        // Pixels bruts non compresses : tableau de aiTexel (BGRA8888).
        // pcData pointe directement vers les pixels, mWidth/mHeight = dimensions.
        width = static_cast<int>(texture->mWidth);
        height = static_cast<int>(texture->mHeight);
        data = reinterpret_cast<unsigned char*>(texture->pcData);
        nrChannels = 4; // aiTexel = 4 composantes (b,g,r,a)
        ownsData = false; // appartient a Assimp, ne pas liberer
    }

    if (data && width > 0 && height > 0) {
        GLenum internalFormat, srcFormat;
        if (texture->mHeight != 0) {
            // aiTexel est BGRA : on uploade en GL_BGRA (disponible en GL 3.3).
            internalFormat = GL_RGBA;
            srcFormat = GL_BGRA;
        }
        else if (nrChannels == 1) {
            internalFormat = GL_RED;
            srcFormat = GL_RED;
        }
        else if (nrChannels == 3) {
            internalFormat = GL_RGB;
            srcFormat = GL_RGB;
        }
        else {
            internalFormat = GL_RGBA;
            srcFormat = GL_RGBA;
        }

        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0,
                     srcFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        std::cout << "Texture embarquee chargee: " << cacheKey
                  << " (" << width << "x" << height << ")" << std::endl;
    }
    else {
        std::cerr << "[Model] Echec decodage texture embarquee: " << cacheKey << std::endl;
        // Texture magenta de debug (cohérent avec loadTextureFromFile).
        unsigned char pink[] = { 255, 0, 255, 255 };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pink);
    }

    if (ownsData && data) {
        stbi_image_free(data);
    }

    m_loadedTextures[cacheKey] = textureID;
    return textureID;
}

void Model::calculateBoundingBox() {
    m_boundingBox = BoundingBox();

    for (auto* mesh : m_meshes) {
        const std::vector<Vertex>& vertices = mesh->getVertices();
        for (const auto& vertex : vertices) {
            m_boundingBox.expand(vertex.getPositions());
        }
    }
}

void Model::calculateBoundingSphere() {
    m_boundingSphere = BoundingSphere(m_boundingBox);
}

void Model::createDebugBoundingBoxMesh() {
    glm::vec3 min = m_boundingBox.min;
    glm::vec3 max = m_boundingBox.max;

    // 8 sommets de la box
    std::vector<Vertex> vertices = {
        Vertex(min.x, min.y, min.z),
        Vertex(max.x, min.y, min.z),
        Vertex(max.x, max.y, min.z),
        Vertex(min.x, max.y, min.z),
        Vertex(min.x, min.y, max.z),
        Vertex(max.x, min.y, max.z),
        Vertex(max.x, max.y, max.z),
        Vertex(min.x, max.y, max.z)
    };

    // Indices pour dessiner les lignes de la box
    std::vector<unsigned int> indices = {
        // Face avant
        0, 1, 1, 2, 2, 3, 3, 0,
        // Face arri�re
        4, 5, 5, 6, 6, 7, 7, 4,
        // Connexions
        0, 4, 1, 5, 2, 6, 3, 7
    };

    m_debugBoundingBoxMesh = new Mesh(vertices, indices);
}

unsigned int Model::loadTextureFromFile(const std::string& path) {
    // Cache : ne pas recharger la m�me texture
    auto it = m_loadedTextures.find(path);
    if (it != m_loadedTextures.end()) return it->second;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(false); // Assimp le fait d�j� via aiProcess_FlipUVs
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);

    if (data) {
        GLenum format;
        if (nrChannels == 1) format = GL_RED;
        else if (nrChannels == 3) format = GL_RGB;
        else                      format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        std::cout << "Texture charg�e: " << path << std::endl;
    }
    else {
        std::cerr << "�chec chargement: " << path << std::endl;
        // Texture magenta de debug
        unsigned char pink[] = { 255, 0, 255, 255 };
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pink);
    }

    stbi_image_free(data);
    m_loadedTextures[path] = textureID;
    return textureID;
}