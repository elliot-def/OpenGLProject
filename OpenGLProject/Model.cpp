#include "Model.h"
#include "Mesh.h"
#include "Shader.h"
#include "TextureManager.h"
#include "Vertex.h"
#include "Camera.h"
#include "LightManager.h"
#include "GlbAnimationRepair.h"

#include <cmath>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <utility>
#include <vector>

#include <glad/glad.h>
#include <stb/stb_image.h>

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

    // Pour le rayon, on prend l'échelle maximale
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
    // Test rapide avec les sphères
    BoundingSphere thisSphere = getTransformedBoundingSphere(thisMatrix);
    BoundingSphere otherSphere = other.getTransformedBoundingSphere(otherMatrix);

    if (!thisSphere.intersects(otherSphere)) {
        return false; // Pas de collision
    }

    // Test précis avec les boxes
    BoundingBox thisBox = getTransformedBoundingBox(thisMatrix);
    BoundingBox otherBox = other.getTransformedBoundingBox(otherMatrix);

    return thisBox.intersects(otherBox);
}

bool Model::raycast(const glm::vec3& origin, const glm::vec3& direction,
    const glm::mat4& modelMatrix, float& distance) const {
    // Transformer le rayon dans l'espace du modèle
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
// Animations externes (FBX/GLB separes du modele, ex: Mixamo)
// ---------------------------------------------------------------------------

void Model::loadExternalAnimations(const std::vector<std::string>& paths) {
    for (const auto& path : paths) {
        auto importer = std::make_unique<Assimp::Importer>();
        // On ne veut QUE les animations, pas la geometrie. Sans flags,
        // Assimp ne traite pas les meshes/textures → gain memoire/perf.
        const aiScene* extScene = importer->ReadFile(path, 0);
        if (!extScene) {
            std::cerr << "[Model] Echec chargement animation externe: " << path
                      << " - " << importer->GetErrorString() << std::endl;
            continue;
        }
        for (unsigned int i = 0; i < extScene->mNumAnimations; i++) {
            m_externalAnimations.push_back(extScene->mAnimations[i]);
        }
        m_externalImporters.push_back(std::move(importer));
    }
    if (!m_externalAnimations.empty()) {
        std::cout << "[Model] " << m_externalAnimations.size()
                  << " animations externes chargees" << std::endl;
    }
}

const aiAnimation* Model::getAnimation(size_t index) const {
    if (!m_scene) return nullptr;
    if (index < m_scene->mNumAnimations)
        return m_scene->mAnimations[index];
    index -= m_scene->mNumAnimations;
    if (index < m_externalAnimations.size())
        return m_externalAnimations[index];
    return nullptr;
}

size_t Model::getNumAnimations() const {
    size_t n = m_scene ? m_scene->mNumAnimations : 0;
    return n + m_externalAnimations.size();
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
        // Déduplication : certains exports GLB/GLTF référencent le même mesh
        // depuis plusieurs nœuds (fréquent avec Mixamo/Blender). Sans ce skip,
        // deux Mesh* identiques sont dessinés à la même position → z-fighting.
        if (m_processedMeshIndices.count(meshIndex)) {
            std::cout << "[Model] Mesh duplicate ignore (index=" << meshIndex
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

    // Traiter récursivement les enfants
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

        // Coordonnées de texture
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
        // FBX : certaines textures embarquees sont exposees sous UNKNOWN
        if (diffuse.empty()) {
            diffuse = loadMaterialTextures(material, aiTextureType_UNKNOWN, scene);
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

        // 2. CORRECTION : Si aucune spéculaire n'est trouvée, on génère/affecte un ID de texture noire
        if (specular.empty()) {
            unsigned int blackTextureID;
            glGenTextures(1, &blackTextureID);
            glBindTexture(GL_TEXTURE_2D, blackTextureID);
            unsigned char blackPixel[] = { 10, 10, 10, 255 }; // Gris très sombre (faible spécularité par défaut)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, blackPixel);

            textureIDs.push_back(blackTextureID);
        }
        else {
            textureIDs.push_back(specular[0]);
        }
    }

    // Binder la texture diffuse avant de créer le mesh (si ton Mesh::draw() utilise l'unité 0)
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

void Model::buildCulledIndices(const std::unordered_set<std::string>& keepBonePatterns) {
    if (keepBonePatterns.empty()) return;

    // boneId -> nom du bone (pour résoudre le bone principal de chaque sommet)
    std::vector<std::string> boneNames(static_cast<size_t>(m_boneCounter));
    for (const auto& [name, info] : m_boneInfoMap) {
        if (info.id >= 0 && info.id < static_cast<int>(boneNames.size())) {
            boneNames[static_cast<size_t>(info.id)] = name;
        }
    }

    for (Mesh* mesh : m_meshes) {
        const std::vector<Vertex>& vertices = mesh->getVertices();
        const std::vector<unsigned int>& indices = mesh->getIndices();
        if (vertices.empty() || indices.empty()) continue;

        // Bone principal de chaque sommet (poids max), -1 si aucun poids
        std::vector<int> primaryBone(vertices.size(), -1);
        for (size_t i = 0; i < vertices.size(); i++) {
            const Vertex& v = vertices[i];
            int best = -1;
            float bestW = 0.0f;
            for (int j = 0; j < MAX_BONE_INFLUENCE; j++) {
                if (v.m_weights[j] > bestW) {
                    bestW = v.m_weights[j];
                    best = v.m_boneIDs[j];
                }
            }
            if (bestW > 0.0f && best >= 0 && best < static_cast<int>(boneNames.size())) {
                primaryBone[i] = best;
            }
        }

        // Sommet gardé si le nom de son bone principal matche un pattern
        std::vector<bool> vertexKept(vertices.size(), false);
        for (size_t i = 0; i < vertices.size(); i++) {
            if (primaryBone[i] < 0) continue;
            const std::string& name = boneNames[static_cast<size_t>(primaryBone[i])];
            for (const auto& pat : keepBonePatterns) {
                if (name.find(pat) != std::string::npos) {
                    vertexKept[i] = true;
                    break;
                }
            }
        }

        // Triangle gardé si ses 3 sommets sont gardés (les indices sont des
        // triangles : aiProcess_Triangulate est actif au chargement)
        std::vector<unsigned int> culled;
        culled.reserve(indices.size() / 2);
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            const unsigned int a = indices[i];
            const unsigned int b = indices[i + 1];
            const unsigned int c = indices[i + 2];
            if (a < vertexKept.size() && b < vertexKept.size() && c < vertexKept.size()
                && vertexKept[a] && vertexKept[b] && vertexKept[c]) {
                culled.push_back(a);
                culled.push_back(b);
                culled.push_back(c);
            }
        }
        mesh->setCulledIndices(culled);
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

        // Si le fichier n'existe pas sur disque, chercher dans les textures
        // embarquees du FBX (scene->mTextures[]) par correspondance de nom.
        if (!std::filesystem::exists(fullPath) && scene) {
            bool foundEmb = false;
            for (unsigned int e = 0; e < scene->mNumTextures; e++) {
                const aiTexture* emb = scene->mTextures[e];
                if (!emb) continue;
                // aiTexture::mFilename est souvent vide pour les FBX
                // embarques ; on identifie par l'index si le chemin est "*N"
                // ou par le nom de fichier original (stocke dans mFilename
                // pour certains formats, ou via achFormatHint).
                std::string embName(emb->mFilename.C_Str());
                if (!embName.empty() && embName == texPath) {
                    textureIDs.push_back(loadEmbeddedTexture(emb, embName));
                    foundEmb = true;
                    break;
                }
            }
            if (!foundEmb) {
                // Dernier essai : si la texture est referencee sans "*"
                // mais que le fichier n'existe pas, peut-etre qu'Assimp l'a
                // extraite sous un nom generic ; on essaie toutes les
                // textures embarquees dont le nom correspond partiellement.
                for (unsigned int e = 0; e < scene->mNumTextures; e++) {
                    const aiTexture* emb = scene->mTextures[e];
                    if (!emb || emb->mHeight == 0) continue; // textures compressees seulement
                    std::string embName(emb->mFilename.C_Str());
                    // Cherche le nom de fichier (sans chemin) dans le nom embarque
                    std::string texFilename = texFsPath.filename().string();
                    if (!embName.empty() && embName.find(texFilename) != std::string::npos) {
                        textureIDs.push_back(loadEmbeddedTexture(emb, embName));
                        foundEmb = true;
                        break;
                    }
                }
            }
            if (foundEmb) continue;
        }

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
        // Face arrière
        4, 5, 5, 6, 6, 7, 7, 4,
        // Connexions
        0, 4, 1, 5, 2, 6, 3, 7
    };

    m_debugBoundingBoxMesh = new Mesh(vertices, indices);
}

unsigned int Model::loadTextureFromFile(const std::string& path) {
    // Cache : ne pas recharger la même texture
    auto it = m_loadedTextures.find(path);
    if (it != m_loadedTextures.end()) return it->second;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(false); // Assimp le fait déjà via aiProcess_FlipUVs
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

        std::cout << "Texture chargee: " << path << std::endl;
    }
    else {
        std::cerr << "Echec chargement: " << path << std::endl;
        // Texture magenta de debug
        unsigned char pink[] = { 255, 0, 255, 255 };
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pink);
    }

    stbi_image_free(data);
    m_loadedTextures[path] = textureID;
    return textureID;
}