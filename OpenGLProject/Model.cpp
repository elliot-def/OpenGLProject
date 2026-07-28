#include "Model.h"
#include "Mesh.h"
#include "Shader.h"
#include "TextureManager.h"
#include "Vertex.h"
#include "Camera.h"
#include "LightManager.h"

#include <iostream>
#include <exception>
#include <utility> // std::declval pour le static_assert(noexcept(Mesh::~Mesh)).

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
    : m_camera(camera), m_lightManager(lightManager), m_textureManager(textureManager), m_debugBoundingBoxMesh(nullptr) {
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
    // restent sur le tas jusqu'a la fin du process). m_importer detruit le
    // aiScene et les aiNode automatiquement (son destructeur propre gere ca),
    // mais les Mesh* que nous avons convertis en C++ doivent etre deletes ici.
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
    m_scene = m_importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals
        // Pas de LimBoneWeights (on garde MAX_BONE_INFLUENCE=4 dans le shader)
        // Les poids sont deja normalises par Assimp : sum ~= 1.0.
    );
    const aiScene* scene = m_scene;

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "ERREUR::ASSIMP::" << m_importer.GetErrorString() << std::endl;
        return;
    }

    // Stocker le dossier physique du fichier .obj
    std::filesystem::path p = path;
    m_directory = p.parent_path().string(); // ex: "./res/models/backpack"

    // Skinning / animations : on extrait les bones par mesh et les animations
    // du scene apres que tous les meshes ont ete processe (ainsi la boneInfoMap
    // partage la meme numerotation pour tous les meshes du modele).
    loadAnimations(scene);

    processNode(scene->mRootNode, scene);
}

void Model::processNode(aiNode* node, const aiScene* scene) {
    // Traiter tous les meshes du noeud
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        m_meshes.push_back(processMesh(mesh, scene));
    }

    // Traiter r�cursivement les enfants
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
            // Optionnel : tu peux lui assigner une texture par d�faut si vide
        }
        else {
            textureIDs.push_back(diffuse[0]);
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

    // IMPORTANT : le peuplement des bone IDs / weights doit avoir lieu AVANT
    // la construction du Mesh (qui appelle glBufferData avec sizeof(Vertex)).
    // Sinon la VBO GPU serait uploadee avec tous bone IDs a 0 et le shader
    // skinned recevrait une identite -> l'animation ne produirait aucun effet.
    if (mesh->mNumBones > 0) {
        extractBoneDataFromMesh(mesh, vertices);
    }

    return new Mesh(vertices, indices, mask, textureIDs);
}

void Model::extractBoneDataFromMesh(const aiMesh* mesh, std::vector<Vertex>& vertices) {
    // Pour chaque bone declare par assimp sur ce mesh :
    //  1. Resoudre / creer un BoneInfo (id unique global).
    //  2. Pour chaque vertex reference par ce bone, ajouter une influence.
    for (unsigned int boneIdx = 0; boneIdx < mesh->mNumBones; ++boneIdx) {
        const aiBone* aiBone = mesh->mBones[boneIdx];
        std::string boneName(aiBone->mName.C_Str());
        int boneID = -1;

        // Lookup ou allocation d'un id pour ce bone (global au modele).
        auto it = m_boneInfoMap.find(boneName);
        if (it == m_boneInfoMap.end()) {
            BoneInfo info;
            info.id = m_boneCounter++;
            info.offsetMatrix = aiMatrixToGlm(aiBone->mOffsetMatrix);
            m_boneInfoMap[boneName] = info;
            boneID = info.id;
        }
        else {
            boneID = it->second.id;
        }

        // Pour chaque influence (vertex, weight) sur ce bone, on accumule
        // dans la liste d'influences du vertex (jusqu'a MAX_BONE_INFLUENCE).
        for (unsigned int w = 0; w < aiBone->mNumWeights; ++w) {
            unsigned int vertexId = aiBone->mWeights[w].mVertexId;
            float weight = aiBone->mWeights[w].mWeight;
            if (vertexId >= vertices.size()) continue;

            Vertex& v = vertices[vertexId];
            // Cherche un slot libre dans m_weights.
            for (int slot = 0; slot < MAX_BONE_INFLUENCE; ++slot) {
                if (v.m_weights[slot] == 0.0f) {
                    v.m_boneIDs[slot] = boneID;
                    v.m_weights[slot] = weight;
                    break;
                }
            }
        }
    }
}

void Model::loadAnimations(const aiScene* scene) {
    m_animations.clear();
    if (!scene) return;

    for (unsigned int i = 0; i < scene->mNumAnimations; ++i) {
        const aiAnimation* aiAnim = scene->mAnimations[i];
        AnimationClip clip;
        clip.name = std::string(aiAnim->mName.C_Str());
        clip.duration = static_cast<double>(aiAnim->mDuration);
        clip.ticksPerSecond = (aiAnim->mTicksPerSecond != 0.0)
                            ? static_cast<double>(aiAnim->mTicksPerSecond)
                            : 25.0; // fallback defensif
        clip.channels.reserve(aiAnim->mNumChannels);

        for (unsigned int ch = 0; ch < aiAnim->mNumChannels; ++ch) {
            const aiNodeAnim* nodeAnim = aiAnim->mChannels[ch];
            AnimationChannel channel;
            channel.nodeName = std::string(nodeAnim->mNodeName.C_Str());

            channel.positionKeys.reserve(nodeAnim->mNumPositionKeys);
            for (unsigned int k = 0; k < nodeAnim->mNumPositionKeys; ++k) {
                PositionKey pk;
                pk.time = static_cast<double>(nodeAnim->mPositionKeys[k].mTime);
                pk.value = glm::vec3(
                    nodeAnim->mPositionKeys[k].mValue.x,
                    nodeAnim->mPositionKeys[k].mValue.y,
                    nodeAnim->mPositionKeys[k].mValue.z);
                channel.positionKeys.push_back(pk);
            }

            channel.rotationKeys.reserve(nodeAnim->mNumRotationKeys);
            for (unsigned int k = 0; k < nodeAnim->mNumRotationKeys; ++k) {
                RotationKey rk;
                rk.time = static_cast<double>(nodeAnim->mRotationKeys[k].mTime);
                rk.value = glm::quat(
                    nodeAnim->mRotationKeys[k].mValue.w,
                    nodeAnim->mRotationKeys[k].mValue.x,
                    nodeAnim->mRotationKeys[k].mValue.y,
                    nodeAnim->mRotationKeys[k].mValue.z);
                channel.rotationKeys.push_back(rk);
            }

            channel.scaleKeys.reserve(nodeAnim->mNumScalingKeys);
            for (unsigned int k = 0; k < nodeAnim->mNumScalingKeys; ++k) {
                ScaleKey sk;
                sk.time = static_cast<double>(nodeAnim->mScalingKeys[k].mTime);
                sk.value = glm::vec3(
                    nodeAnim->mScalingKeys[k].mValue.x,
                    nodeAnim->mScalingKeys[k].mValue.y,
                    nodeAnim->mScalingKeys[k].mValue.z);
                channel.scaleKeys.push_back(sk);
            }

            clip.channels.push_back(std::move(channel));
        }

        m_animations.push_back(std::move(clip));
    }
}

std::vector<unsigned int> Model::loadMaterialTextures(aiMaterial* mat, aiTextureType type) {
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
            const aiTexture* emb = m_scene ? m_scene->GetEmbeddedTexture(texPath.c_str()) : nullptr;
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