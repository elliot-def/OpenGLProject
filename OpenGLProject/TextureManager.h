#pragma once
#include <vector>
#include <stdexcept>
#include <string>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <unordered_map>
#include <nlohmann/json.hpp>

#include "constants/Texture.h"

using json = nlohmann::json;

class Texture; // Déclaration anticipée pour éviter les include circulaires

// Structure pour stocker les informations d'une texture
struct  TextureInfo {
    std::string texturePath;
    std::string specularPath;
    std::string shininessPath;
    float shininess;
    bool hasSpecular;
    bool hasShininess;
};

// Structure représentant un noeud de l'arborescence des textures
struct TextureNode {
    std::unordered_map<std::string, TextureNode*> children; // sous-dossiers
    Texture* texture = nullptr; // pointeur vers la texture si c'est une feuille
};

/**
 * @class TextureManager
 * @brief Charge et gère toutes les textures d'un dossier pour OpenGL
 *
 * Cette classe fait 3 choses principales :
 * 1. Parcourt un dossier donné et charge automatiquement toutes les images .png en tant que textures OpenGL.
 * 2. Organise les textures dans une arborescence (comme un explorateur de fichiers).
 * 3. Permet d'accéder à une texture facilement via son chemin relatif.
 *
 * Notes importantes :
 * - Les textures sont stockées en pointeurs bruts (Texture*). Le destructeur supprime
 *   ces pointeurs pour éviter les fuites mémoire.
 * - L'utilisation de std::filesystem (C++17) facilite le parcours des dossiers.
 */
class TextureManager {

public:
    /**
     * @brief Constructeur
     *
     * Charge automatiquement toutes les textures du dossier par défaut (Constants::TEXTURES_FOLDER_PATH)
     */
    TextureManager();

    /**
     * @brief Destructeur
     *
     * Libère toutes les textures et tous les noeuds de l'arborescence.
     */
    ~TextureManager();

    /**
     * @brief Récupère une texture via son chemin relatif
     * @param path Chemin relatif (ex: "herbe.png" ou "dossier/sousdossier/texture.png")
     * @return Texture* Pointeur vers la texture
     * @throws std::out_of_range si le chemin est invalide ou la texture n'existe pas
     *
     * Exemple :
     * @code
     * Texture* tex = textureManager.getTexture("herbe.png");
     * glBindTexture(GL_TEXTURE_2D, tex->getID());
     * @endcode
     */
    Texture* getTexture(const std::string& path);

    /// Texture blanche 1x1 partagée (fallback diffuse). Réutilisée par
    /// Model::processMesh pour les meshes sans texture diffuse, au lieu de
    /// générer une nouvelle texture par mesh.
    unsigned int getDefaultDiffuseID() const { return m_defaultDiffuseID; }

    /// Texture noire/gris foncé 1x1 partagée (fallback spéculaire).
    unsigned int getDefaultSpecularID() const { return m_defaultSpecularID; }

    /**
     * @brief Affiche l'arborescence complète des textures dans la console
     *
     * Format d'affichage :
     * └── dossier1/
     *     ├── texture1.png [ID: 0, shininess: 32.0]
     *     └── sous-dossier/
     *         └── texture2.png [ID: 2, shininess: 64.0, specular]
     */
    void printTextureTree() const;
private:
    TextureNode m_root; ///< Racine de l'arborescence des textures
    unsigned int m_defaultSpecularID = Constants::Texture::BLACK_TEXTURE_ID;  // Texture par défaut
    // Placeholder (écrasé par createDefaultTextures() via glGenTextures) :
    // l'ID réel de la texture blanche 1x1 par défaut (diffuse).
    unsigned int m_defaultDiffuseID  = Constants::Texture::BLACK_TEXTURE_ID;

    /**
     * @brief Charge toutes les textures depuis des dossiers donnés
     * @param texturesFolderPath Chemin du dossier (par défaut : Constants::TEXTURES_FOLDER_PATH)
     *
     * Étapes du chargement :
     * 1. Vérifie que le dossier existe
     * 2. Parcourt tous les fichiers récursivement
     * 3. Pour chaque fichier .png, crée un objet Texture et l'ajoute à l'arborescence
     */
    void loadTextures(std::span<const char* const> texturesFolderPath = Constants::Texture::TEXTURES_FOLDER_PATHS);

    void createDefaultTextures();

    /**
     * @brief Supprime récursivement tous les noeuds et textures de l'arborescence
     * @param node Pointeur vers le noeud à supprimer
     */
    void deleteNode(TextureNode* node);

    /**
     * @brief Fonction récursive pour afficher l'arborescence
     * @param node Noeud actuel à afficher
     * @param prefix Préfixe pour l'indentation
     * @param isLast Indique si c'est le dernier enfant
     */
    void printNode(const TextureNode* node, const std::string& prefix = "", bool isLast = true) const;

    // Fonctions privées pour décomposer la logique
    float loadTextureProperties(const std::filesystem::path& jsonPath);

    TextureInfo getTextureInfoFromFolder(const std::filesystem::path& folderPath);

    void createTextureNode(const std::string& relativePath, const TextureInfo& info, int& textureIDCounter);

    bool loadTextureFromFolder(const std::filesystem::path& folderPath, const std::string& texturesFolderPath, int& textureIDCounter);

};
