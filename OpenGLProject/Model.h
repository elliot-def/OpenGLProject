#pragma once

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "Vertex.h"
#include "SkinningData.h"

class Shader;
class Mesh;
class TextureManager;
class Camera;
class LightManager;

#pragma comment(lib, "assimp-vc143-mtd.lib")

// Structure pour repr�senter une bounding box
struct BoundingBox {
    glm::vec3 min;
    glm::vec3 max;

    BoundingBox() : min(FLT_MAX), max(-FLT_MAX) {}

    void expand(const glm::vec3& point) {
        min = (glm::min)(min, point);
        max = (glm::max)(max, point);
    }

    glm::vec3 getCenter() const {
        return (min + max) * 0.5f;
    }

    glm::vec3 getSize() const {
        return max - min;
    }

    // Transforme la bounding box avec une matrice
    BoundingBox transform(const glm::mat4& matrix) const;

    // V�rifie la collision avec une autre bounding box
    bool intersects(const BoundingBox& other) const;
};

// Structure pour une hitbox sph�rique (plus simple pour certains objets)
struct BoundingSphere {
    glm::vec3 center;
    float radius;

    BoundingSphere() : center(0.0f), radius(0.0f) {}
    BoundingSphere(const BoundingBox& box);

    // Transforme la sph�re avec une matrice
    BoundingSphere transform(const glm::mat4& matrix) const;

    // V�rifie la collision avec une autre sphere
    bool intersects(const BoundingSphere& other) const;
};

class Model {
public:
    // Constructeur avec chemin du modele et texture manager
    Model(Camera* camera, LightManager* lightManager, const std::string& path, TextureManager* textureManager = nullptr);
    ~Model(); // Libere les Mesh* heap-allocated via new (processMesh + createDebugBoundingBoxMesh).

    // Dessine le mod�le
    void draw(Shader& shader);

    // Dessine la bounding box (pour debug)
    void drawBoundingBox(Shader& shader);

    const std::vector<Mesh*>& getMeshes() const { return m_meshes; }

    // Accès au skeleton et animations
    const aiScene* getScene() const { return m_scene; }
    const aiNode*  getRootNode() const { return m_scene ? m_scene->mRootNode : nullptr; }
    const std::string& getSourcePath() const { return m_sourcePath; }
    const std::unordered_map<std::string, BoneInfo>& getBoneInfoMap() const { return m_boneInfoMap; }

    // Noeuds joints du squelette CONSERVE (le premier de l'ordre du fichier).
    // L'Animator n'ecrit les bone matrices que pour ces noeuds : les autres
    // squelettes du fichier (ex: FemaleArm dans human_1.glb) partagent les
    // MEMES noms de bones mais ne doivent pas ecraser les matrices du premier.
    const std::unordered_set<const aiNode*>& getJointNodes() const { return m_jointNodes; }

    // Getters pour les hitbox
    const BoundingBox& getBoundingBox() const { return m_boundingBox; }
    const BoundingSphere& getBoundingSphere() const { return m_boundingSphere; }

    // Obtenir la bounding box transform�e
    BoundingBox getTransformedBoundingBox(const glm::mat4& modelMatrix) const;
    BoundingSphere getTransformedBoundingSphere(const glm::mat4& modelMatrix) const;

    // V�rifier collision avec un autre mod�le
    bool checkCollision(const Model& other, const glm::mat4& thisMatrix,
        const glm::mat4& otherMatrix) const;

    // Raycast - renvoie true si le rayon touche le mod�le
    bool raycast(const glm::vec3& origin, const glm::vec3& direction,
        const glm::mat4& modelMatrix, float& distance) const;

    // Animations externes (FBX/GLB separes du modele, ex: Mixamo)
    void loadExternalAnimations(const std::vector<std::string>& paths);
    const aiAnimation* getAnimation(size_t index) const;
    size_t getNumAnimations() const;
    // Nombre de clips externes (chargés via loadExternalAnimations). Les
    // animations embarquées du modèle occupent les index [0, getNumAnimations()-externes[.
    size_t getNumExternalAnimations() const { return m_externalAnimations.size(); }

private:
    std::string m_sourcePath;
    std::string m_directory;
    std::string m_texturesDirectory;

    TextureManager* m_textureManager;
    Camera* m_camera;
    LightManager* m_lightManager;

    std::unordered_map<std::string, unsigned int> m_loadedTextures;
    std::vector<Mesh*> m_meshes;

    // Hitbox du mod�le entier
    BoundingBox m_boundingBox;
    BoundingSphere m_boundingSphere;

    // Mesh pour visualiser la bounding box
    Mesh* m_debugBoundingBoxMesh;

    // Importer + scene conservés pour que le aiScene* reste adressable
    Assimp::Importer m_importer;
    const aiScene*   m_scene = nullptr;

    // Bone mapping global partagé par tous les meshes du modèle
    std::unordered_map<std::string, BoneInfo> m_boneInfoMap;
    int m_boneCounter = 0;

    // D�duplication : certains exports GLB/GLTF r�f�rencent le m�me aiMesh
    // depuis plusieurs nœuds (fr�quent avec Mixamo/Blender). Sans ce set,
    // deux Mesh* identiques sont pouss�s dans m_meshes → z-fighting � l'�cran.
    std::unordered_set<unsigned int> m_processedMeshIndices;

    // ── Animations externes ───────────────────────────────────────────
    // Importers pour les fichiers d'animation externes (doivent survivre
    // aux aiAnimation* qu'ils contiennent — detruits avec ce Model).
    // unique_ptr car Assimp::Importer n'est pas copiable/movable.
    std::vector<std::unique_ptr<Assimp::Importer>> m_externalImporters;
    std::vector<const aiAnimation*> m_externalAnimations;

    // Noeuds joints du squelette conserve (voir getJointNodes()). Construit
    // apres processNode() : parcours depth-first de l'arbre, on reclame le
    // PREMIER noeud de chaque nom de bone enregistre. Comme l'enregistrement
    // des bones (processMesh) suit le meme ordre, c'est le squelette du mesh
    // conserve — les noeuds homonymes des autres rigs sont ignores.
    std::unordered_set<const aiNode*> m_jointNodes;
    void buildJointNodes();
    void collectJointNodes(const aiNode* node, std::unordered_set<std::string>& claimed);

    // Chargement
    unsigned int loadTextureFromFile(const std::string& path);
    // Charge une texture embarquee (GLB/GLTF) depuis un aiTexture. Les GLB
    // stockent leurs textures dans le binaire ; Assimp les expose via des
    // chemins "*0", "*1" (index dans scene->mTextures[]). Deux cas :
    //  - mHeight==0 : texture compressee (PNG/JPEG) -> stbi_load_from_memory
    //  - mHeight!=0 : pixels bruts BGRA8888 -> upload direct avec GL_BGRA
    // cacheKey = le chemin "*N" original (sert de cle de cache partagee).
    unsigned int loadEmbeddedTexture(const aiTexture* texture, const std::string& cacheKey);
    void loadModel(const std::string& path);
    void extractBoneDataFromMesh(aiMesh* mesh, std::vector<Vertex>& vertices);
    void processNode(aiNode* node, const aiScene* scene);
    Mesh* processMesh(aiMesh* mesh, const aiScene* scene);

    // Calcul des hitbox
    void calculateBoundingBox();
    void calculateBoundingSphere();

    // Cr�ation du mesh de debug
    void createDebugBoundingBoxMesh();

    // Helpers pour les textures
    std::vector<unsigned int> loadMaterialTextures(aiMaterial* mat,
        aiTextureType type, const aiScene* scene);
};