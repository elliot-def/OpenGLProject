#include "TextureManager.h"
#include "Texture.h"

#include "constants/texture.h"
#include "constants/material.h"
#include "constants/file.h"

#include <fstream>
#include <glad/glad.h>

TextureManager::TextureManager() {
    createDefaultTextures();
	loadTextures();
    printTextureTree();
}

TextureManager::~TextureManager() {
    deleteNode(&m_root);
    glDeleteTextures(1, &m_defaultSpecularID);
};

Texture* TextureManager::getTexture(const std::string& path) {
    std::stringstream ss(path);
    std::string part;
    TextureNode* current = &m_root;

    char sep = static_cast<char>(Constants::File::PREFERED_SEPARATOR_PATH);
    while (std::getline(ss, part, sep)) {
        auto it = current->children.find(part);
        if (it == current->children.end()) {
            throw std::out_of_range("Path not found: " + path);
        }
        current = it->second;
    }

    if (!current->texture) {
        throw std::out_of_range("No texture at path: " + path);
    }
    return current->texture;
}

// Fonction pour charger les propriétés depuis le JSON
float TextureManager::loadTextureProperties(const std::filesystem::path& jsonPath) {
    try {
        std::ifstream jsonFile(jsonPath);
        json jsonData = json::parse(jsonFile);
        return jsonData.value("shininess", Constants::Material::PLASTIC_GLOSSY);
    }
    catch (const json::exception& e) {
        std::cerr << "Erreur parsing JSON " << jsonPath << ": " << e.what() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Erreur ouverture JSON " << jsonPath << ": " << e.what() << std::endl;
    }

    return Constants::Material::PLASTIC_GLOSSY;
}

// Fonction pour récupérer les informations d'une texture depuis un dossier
TextureInfo TextureManager::getTextureInfoFromFolder(const std::filesystem::path& folderPath) {
    TextureInfo info;
    std::string folderName = folderPath.filename().string();

    info.texturePath = (folderPath / (folderName + ".png")).string();
    info.specularPath = (folderPath / (folderName + "_specular.png")).string();
    info.hasSpecular = std::filesystem::exists(info.specularPath);

    info.shininessPath = (folderPath / (folderName + ".json")).string();
    info.hasShininess = std::filesystem::exists(info.shininessPath);
    if (info.hasShininess) {
        info.shininess = loadTextureProperties(info.shininessPath);
    }
    else {
        std::cerr << "Fichier de proprietes manquant pour " << folderName << ": " << info.shininessPath << std::endl;
        info.shininess = Constants::Material::PLASTIC_GLOSSY;
	}

    return info;
}

// Fonction pour créer un noeud dans l'arborescence
void TextureManager::createTextureNode(const std::string& relativePath, const TextureInfo& info, int& textureIDCounter) {
    TextureNode* current = &m_root;
    std::stringstream ss(relativePath);
    std::string part;
    char sep = static_cast<char>(std::filesystem::path::preferred_separator);

    while (std::getline(ss, part, sep)) {
        if (ss.peek() == EOF) {
            // Dernier segment : créer la texture
            current->children[part] = new TextureNode();
            current->children[part]->texture = new Texture(
                info.texturePath,
                textureIDCounter,
                info.shininess,
                info.hasSpecular
            );

            textureIDCounter++;
            if (info.hasSpecular) textureIDCounter++;
        }
        else {
            // Segment intermédiaire : créer un noeud
            if (current->children.find(part) == current->children.end()) {
                current->children[part] = new TextureNode();
            }
            current = current->children[part];
        }
    }
}

// Fonction pour charger une texture depuis un dossier
bool TextureManager::loadTextureFromFolder(const std::filesystem::path& folderPath, const std::string& texturesFolderPath, int& textureIDCounter) {
    std::string folderName = folderPath.filename().string();

    // Récupérer les informations de la texture
    TextureInfo info = getTextureInfoFromFolder(folderPath);

    // Vérifier que la texture principale existe
    if (!std::filesystem::exists(info.texturePath)) {
        std::cerr << "Texture principale manquante: " << info.texturePath << std::endl;
        return false;
    }

    // Créer le noeud dans l'arborescence
    std::string relativePath = folderPath.lexically_relative(texturesFolderPath).string();
    createTextureNode(relativePath, info, textureIDCounter);

    // Log
    std::cout << "Texture chargee: " << folderName
        << " (shininess: " << info.shininess
        << ", specular: " << (info.hasSpecular ? "oui" : "non") << ")"
		<< ", shininess file: " << (info.hasShininess ? info.shininessPath : "non trouvé")
        << std::endl;

    return true;
}

void TextureManager::loadTextures(std::span<const char* const> texturesFolderPath) {
    size_t count = std::size(texturesFolderPath);

    int textureIDCounter = Constants::Texture::FIRST_TEXTURE_ID;
    
    for (int i = 0; i < count; i++) {
        const char* path = texturesFolderPath[i];

        if (!std::filesystem::is_directory(path)) {
            std::cerr << "Le dossier des textures n'existe pas: " << path << std::endl;
            return;
        }

        

        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (entry.is_directory()) {
                std::string folderName = entry.path().filename().string();

                // Ignorer les dossiers réservés aux modèles 3D
                if (folderName == "models") continue;

                loadTextureFromFolder(entry.path(), path, textureIDCounter);
            }
        }
        i++;
    }
}

void TextureManager::deleteNode(TextureNode* node) {
    for (auto& pair : node->children) {
        deleteNode(pair.second);
        delete pair.second;
    }
    if (node->texture) {
        delete node->texture;
    }
}

void TextureManager::printTextureTree() const {
    std::cout << "\n=== Arborescence des Textures ===" << std::endl;
    if (m_root.children.empty()) {
        std::cout << "(Aucune texture chargee)" << std::endl;
        return;
    }

    size_t count = 0;
    size_t total = m_root.children.size();

    for (const auto& pair : m_root.children) {
        bool isLast = (++count == total);   
        // Remplacement ici
        std::cout << pair.first;

        if (pair.second->texture) {
            std::cout << " [ID: " << pair.second->texture->getID()
                << ", shininess: " << pair.second->texture->getShininess();
            if (pair.second->texture->hasSpecular()) {
                std::cout << ", specular";
            }
            std::cout << "]";
        }
        else {
            std::cout << "/";
        }
        std::cout << std::endl;

        if (!pair.second->children.empty()) {
            std::string newPrefix = isLast ? "    " : "\xE2\x94\x82   ";
            printNode(pair.second, newPrefix, false);
        }
    }
    std::cout << "================================\n" << std::endl;
}

void TextureManager::printNode(const TextureNode* node, const std::string& prefix, bool isLast) const {
    if (!node || node->children.empty()) return;

    size_t count = 0;
    size_t total = node->children.size();

    for (const auto& pair : node->children) {
        bool isLastChild = (++count == total);

        // Remplacement ici
        std::cout << prefix << (isLastChild ? "\xE2\x94\x94\xE2\x94\x80\xE2\x94\x80 " : "\xE2\x94\x9C\xE2\x94\x80\xE2\x94\x80 ") << pair.first;

        if (pair.second->texture) {
            std::cout << " [ID: " << pair.second->texture->getID()
                << ", shininess: " << pair.second->texture->getShininess();
            if (pair.second->texture->hasSpecular()) {
                std::cout << ", specular";
            }
            std::cout << "]";
        }
        else {
            std::cout << "/";
        }
        std::cout << std::endl;

        if (!pair.second->children.empty()) {
            std::string newPrefix = prefix + (isLastChild ? "    " : "\xE2\x94\x82   ");
            printNode(pair.second, newPrefix, false);
        }
    }
}

void TextureManager::createDefaultTextures() {
    // Créer une texture grise pour specular par défaut
    glGenTextures(1, &m_defaultSpecularID);
    glBindTexture(GL_TEXTURE_2D, m_defaultSpecularID);

    unsigned char pixel[4] = { 1, 1, 1, 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    std::cout << "Default specular texture created (ID: " << m_defaultSpecularID << ")" << std::endl;
}
