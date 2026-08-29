#include "Log.h"
#include "TextureManager.h"
#include "Texture.h"

#include "constants/texture.h"
#include "constants/material.h"
#include "constants/file.h"

#include <fstream>
#include <memory>
#include <glad/glad.h>

TextureManager::TextureManager() {
    createDefaultTextures();
	loadTextures();
    printTextureTree();
}

TextureManager::~TextureManager() {
    glDeleteTextures(1, &m_defaultSpecularID);
    glDeleteTextures(1, &m_defaultDiffuseID);
};

Texture* TextureManager::getTexture(const std::string& path) {
    return m_textures.find(path);
}

// Fonction pour charger les propriétés depuis le JSON
float TextureManager::loadTextureProperties(const std::filesystem::path& jsonPath) {
    try {
        std::ifstream jsonFile(jsonPath);
        json jsonData = json::parse(jsonFile);
        return jsonData.value("shininess", Constants::Material::PLASTIC_GLOSSY);
    }
    catch (const json::exception& e) {
        logErr() << "Erreur parsing JSON " << jsonPath << ": " << e.what() << std::endl;
    }
    catch (const std::exception& e) {
        logErr() << "Erreur ouverture JSON " << jsonPath << ": " << e.what() << std::endl;
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
        logErr() << "Fichier de proprietes manquant pour " << folderName << ": " << info.shininessPath << std::endl;
        info.shininess = Constants::Material::PLASTIC_GLOSSY;
	}

    return info;
}

// Fonction pour créer un noeud dans l'arborescence
void TextureManager::createTextureNode(const std::string& relativePath, const TextureInfo& info, int& textureIDCounter) {
    ResourceTree<Texture>::Node& leaf = m_textures.getOrCreateNode(relativePath);
    leaf.value = std::make_unique<Texture>(
        info.texturePath,
        textureIDCounter,
        info.shininess,
        info.hasSpecular
    );

    textureIDCounter++;
    if (info.hasSpecular) textureIDCounter++;
}

// Fonction pour charger une texture depuis un dossier
bool TextureManager::loadTextureFromFolder(const std::filesystem::path& folderPath, const std::string& texturesFolderPath, int& textureIDCounter) {
    std::string folderName = folderPath.filename().string();

    // Récupérer les informations de la texture
    TextureInfo info = getTextureInfoFromFolder(folderPath);

    // Vérifier que la texture principale existe
    if (!std::filesystem::exists(info.texturePath)) {
        logErr() << "Texture principale manquante: " << info.texturePath << std::endl;
        return false;
    }

    // Créer le noeud dans l'arborescence
    std::string relativePath = folderPath.lexically_relative(texturesFolderPath).string();
    createTextureNode(relativePath, info, textureIDCounter);

    // Log
    logOut() << "Texture chargee: " << folderName
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
            logErr() << "Le dossier des textures n'existe pas: " << path << std::endl;
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

void TextureManager::printTextureTree() const {
    std::ostringstream out;
    out << "\n=== Arborescence des Textures ===" << std::endl;
    if (m_textures.root().children.empty()) {
        out << "(Aucune texture chargee)" << std::endl;
        logRaw(out.str());
        return;
    }

    size_t count = 0;
    size_t total = m_textures.root().children.size();

    for (const auto& pair : m_textures.root().children) {
        bool isLast = (++count == total);   
        // Remplacement ici
        out << pair.first;

        if (pair.second->value) {
            out << " [ID: " << pair.second->value->getID()
                << ", shininess: " << pair.second->value->getShininess();
            if (pair.second->value->hasSpecular()) {
                out << ", specular";
            }
            out << "]";
        }
        else {
            out << "/";
        }
        out << std::endl;

        if (!pair.second->children.empty()) {
            std::string newPrefix = isLast ? "    " : "\xE2\x94\x82   ";
            printNode(pair.second.get(), newPrefix, false, out);
        }
    }
    out << "================================\n" << std::endl;
    logRaw(out.str());
}

void TextureManager::printNode(const ResourceTree<Texture>::Node* node, const std::string& prefix, bool isLast, std::ostringstream& out) const {
    if (!node || node->children.empty()) return;

    size_t count = 0;
    size_t total = node->children.size();

    for (const auto& pair : node->children) {
        bool isLastChild = (++count == total);

        // Remplacement ici
        out << prefix << (isLastChild ? "\xE2\x94\x94\xE2\x94\x80\xE2\x94\x80 " : "\xE2\x94\x9C\xE2\x94\x80\xE2\x94\x80 ") << pair.first;

        if (pair.second->value) {
            out << " [ID: " << pair.second->value->getID()
                << ", shininess: " << pair.second->value->getShininess();
            if (pair.second->value->hasSpecular()) {
                out << ", specular";
            }
            out << "]";
        }
        else {
            out << "/";
        }
        out << std::endl;

        if (!pair.second->children.empty()) {
            std::string newPrefix = prefix + (isLastChild ? "    " : "\xE2\x94\x82   ");
            printNode(pair.second.get(), newPrefix, false, out);
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

    logOut() << "Default specular texture created (ID: " << m_defaultSpecularID << ")" << std::endl;

    // Texture BLANCHE 1x1 partagée (fallback diffuse). Réutilisée par
    // Model::processMesh pour les meshes sans texture diffuse.
    glGenTextures(1, &m_defaultDiffuseID);
    glBindTexture(GL_TEXTURE_2D, m_defaultDiffuseID);

    unsigned char whitePixel[4] = { 255, 255, 255, 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    logOut() << "Default diffuse texture created (ID: " << m_defaultDiffuseID << ")" << std::endl;
}
