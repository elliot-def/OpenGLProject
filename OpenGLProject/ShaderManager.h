#pragma once
#include <vector>
#include <stdexcept>
#include <string>
#include <iostream>
#include <filesystem>
#include <memory>

#include "Log.h"
#include "Shader.h"
#include "ResourceTree.h"
#include "constants/shader.h"
#include "constants/file.h"

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
     * @brief Récupère un shader à partir d'un chemin relatif
     * @param path Chemin du shader (ex: "folder/subfolder/shadername")
     * @return Pointeur vers le shader correspondant
     * @throws std::out_of_range si le chemin ou le shader n'existe pas
     */
    Shader* getShader(const std::string& path) {
        return m_shaders.find(path);
    }

private:
    ResourceTree<Shader> m_shaders; ///< Arborescence des shaders (propriétaire)
    Camera* m_camera;               ///< Pointeur vers la caméra

    /**
     * @brief Charge tous les shaders depuis le dossier donné et construit l'arborescence
     * @param shadersFolderPath Dossier racine des shaders (défaut : Constants::Shader::SHADERS_FOLDER_PATH)
     */
    void loadShaders(std::string shadersFolderPath = Constants::Shader::SHADERS_FOLDER_PATH) {
        if (!std::filesystem::is_directory(shadersFolderPath)) {
            logErr() << "Le dossier des shaders n'existe pas: " << shadersFolderPath << std::endl;
            return;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(shadersFolderPath)) {
            if (entry.is_directory()) {
                std::string folderName = entry.path().filename().string();
                auto vertPath = entry.path() / (folderName + ".vert");
                auto fragPath = entry.path() / (folderName + ".frag");

                if (std::filesystem::exists(vertPath) && std::filesystem::exists(fragPath)) {
                    std::string relative = entry.path().lexically_relative(shadersFolderPath).string();
                    ResourceTree<Shader>::Node& leaf = m_shaders.getOrCreateNode(relative);
                    leaf.value = std::make_unique<Shader>(
                        vertPath.string(),
                        fragPath.string(),
                        m_camera,
                        true
                    );
                }
            }
        }
    }
};
