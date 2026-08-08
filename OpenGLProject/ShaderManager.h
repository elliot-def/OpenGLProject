#pragma once
#include <vector>
#include <stdexcept>
#include <string>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <unordered_map>

#include "Shader.h"
#include "constants/shader.h"
#include "constants/file.h"

// Structure représentant un noeud dans l'arborescence des shaders
struct ShaderNode {
    std::unordered_map<std::string, ShaderNode*> children;  ///< Sous-dossiers
    Shader* shader = nullptr;                               ///< Shader associé si c'est une feuille
};

/**
 * @class ShaderManager
 * @brief Gère le chargement et l'organisation des shaders en arborescence
 *
 * Charge tous les shaders présents dans le dossier défini par Constants::SHADERS_FOLDER_PATH
 * et permet d'y accéder via un chemin relatif.
 */
class ShaderManager {
public:
    /**
     * @brief Constructeur : initialise le manager et charge tous les shaders
     * @param camera Pointeur vers la caméra, nécessaire pour initialiser les shaders
     */
    ShaderManager(Camera* camera) : m_camera(camera) { loadShaders(); };

    /**
     * @brief Destructeur : libère tous les shaders et les noeuds de l'arborescence
     */
    ~ShaderManager() {
        deleteNode(&m_root);
    };

    /**
     * @brief Récupère un shader à partir d'un chemin relatif
     * @param path Chemin du shader (ex: "folder/subfolder/shadername")
     * @return Pointeur vers le shader correspondant
     * @throws std::out_of_range si le chemin ou le shader n'existe pas
     */
    Shader* getShader(const std::string& path) {
        std::stringstream ss(path);
        std::string part;
        ShaderNode* current = &m_root;

        char sep = static_cast<char>(Constants::File::PREFERED_SEPARATOR_PATH);
        while (std::getline(ss, part, sep)) {
            auto it = current->children.find(part);
            if (it == current->children.end()) {
                throw std::out_of_range("Path not found: " + path);
            }
            current = it->second;
        }

        if (!current->shader) {
            throw std::out_of_range("No shader at path: " + path);
        }
        return current->shader;
    }

private:
    ShaderNode m_root; ///< Racine de l'arborescence
    Camera* m_camera;  ///< Pointeur vers la caméra

    /**
     * @brief Charge tous les shaders depuis le dossier donné et construit l'arborescence
     * @param shadersFolderPath Dossier racine des shaders (défaut : Constants::Shader::SHADERS_FOLDER_PATH)
     */
    void loadShaders(std::string shadersFolderPath = Constants::Shader::SHADERS_FOLDER_PATH) {
        if (!std::filesystem::is_directory(shadersFolderPath)) {
            std::cerr << "Le dossier des shaders n'existe pas: " << shadersFolderPath << std::endl;
            return;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(shadersFolderPath)) {
            if (entry.is_directory()) {
                std::string folderName = entry.path().filename().string();
                auto vertPath = entry.path() / (folderName + ".vert");
                auto fragPath = entry.path() / (folderName + ".frag");

                if (std::filesystem::exists(vertPath) && std::filesystem::exists(fragPath)) {
                    std::string relative = entry.path().lexically_relative(shadersFolderPath).string();
                    std::stringstream ss(relative);
                    std::string part;
                    ShaderNode* current = &m_root;
                    char sep = static_cast<char>(std::filesystem::path::preferred_separator);

                    while (std::getline(ss, part, sep)) {
                        if (ss.peek() == EOF) {
                            // Dernier dossier : créer le Shader
                            if (current->children.find(part) == current->children.end()) {
                                current->children[part] = new ShaderNode();
                            }
                            current->children[part]->shader = new Shader(
                                vertPath.string(),
                                fragPath.string(),
                                m_camera,
                                true
                            );
                        }
                        else {
                            if (current->children.find(part) == current->children.end()) {
                                current->children[part] = new ShaderNode();
                            }
                            current = current->children[part];
                        }
                    }
                }
            }
        }
    }

    /**
     * @brief Supprime récursivement tous les noeuds et shaders de l'arborescence
     * @param node Pointeur vers le noeud à supprimer
     */
    void deleteNode(ShaderNode* node) {
        for (auto& pair : node->children) {
            deleteNode(pair.second);
            delete pair.second;
        }
        if (node->shader) {
            delete node->shader;
        }
    }
};
