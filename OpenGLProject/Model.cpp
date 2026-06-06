#include "Model.h"
#include "Mesh.h"
#include "Shader.h"
#include "TextureManager.h"
#include "Vertex.h"
#include "Camera.h"
#include "LightManager.h"

#include <iostream>
#include <exception>

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
    : m_camera(camera), m_lightManager(lightManager), m_textureManager(textureManager), m_debugBoundingBoxMesh(nullptr) {
    loadModel(path);
    calculateBoundingBox();
    calculateBoundingSphere();
    createDebugBoundingBoxMesh();
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
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "ERREUR::ASSIMP::" << importer.GetErrorString() << std::endl;
        return;
    }

    // Stocker le dossier physique du fichier .obj
    std::filesystem::path p = path;
    m_directory = p.parent_path().string(); // ex: "./res/models/backpack"

    processNode(scene->mRootNode, scene);
}

void Model::processNode(aiNode* node, const aiScene* scene) {
    // Traiter tous les meshes du noeud
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        m_meshes.push_back(processMesh(mesh, scene));
    }

    // Traiter récursivement les enfants
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        processNode(node->mChildren[i], scene);
    }
}

Mesh* Model::processMesh(aiMesh* mesh, const aiScene* scene) {
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
        auto diffuse = loadMaterialTextures(material, aiTextureType_DIFFUSE);
        auto specular = loadMaterialTextures(material, aiTextureType_SPECULAR);

        // 1. Si aucune diffuse, on met un fallback (ex: texture blanche ou debug)
        if (diffuse.empty()) {
            // Optionnel : tu peux lui assigner une texture par défaut si vide
        }
        else {
            textureIDs.push_back(diffuse[0]);
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

    return new Mesh(vertices, indices, mask, textureIDs);
}

std::vector<unsigned int> Model::loadMaterialTextures(aiMaterial* mat, aiTextureType type) {
    std::vector<unsigned int> textureIDs;

    for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
        aiString str;
        mat->GetTexture(type, i, &str);

        std::string fullPath = m_directory + "/" + std::string(str.C_Str());
        textureIDs.push_back(loadTextureFromFile(fullPath));
    }

    return textureIDs;
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

        std::cout << "Texture chargée: " << path << std::endl;
    }
    else {
        std::cerr << "Échec chargement: " << path << std::endl;
        // Texture magenta de debug
        unsigned char pink[] = { 255, 0, 255, 255 };
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pink);
    }

    stbi_image_free(data);
    m_loadedTextures[path] = textureID;
    return textureID;
}