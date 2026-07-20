#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>

class File {
private:
    std::filesystem::path m_path;

public:
    // Constructeur avec le chemin du fichier
    explicit File(const std::string& filePath) : m_path(filePath) {}

    // Vérifie si le fichier existe
    bool exists() const {
        return std::filesystem::exists(m_path) && std::filesystem::is_regular_file(m_path);
    }

    // Crée un fichier vide s'il n'existe pas
    bool create() {
        if (exists()) return false;
        std::ofstream file(m_path);
        return file.is_open();
    }

    // Supprime le fichier
    bool remove() {
        if (!exists()) return false;
        return std::filesystem::remove(m_path);
    }

    // Renvoie la taille du fichier en octets (bytes)
    uintmax_t size() const {
        if (!exists()) return 0;
        return std::filesystem::file_size(m_path);
    }

    // Écrit du texte dans le fichier (écrase le contenu existant)
    bool writeText(const std::string& content) {
        std::ofstream file(m_path, std::ios::out | std::ios::trunc);
        if (!file.is_open()) return false;
        file << content;
        return true;
    }

    // Ajoute du texte à la fin du fichier (mode "append")
    bool appendText(const std::string& content) {
        std::ofstream file(m_path, std::ios::out | std::ios::app);
        if (!file.is_open()) return false;
        file << content;
        return true;
    }

    // Lit tout le contenu du fichier d'un coup
    std::string readAll() const {
        if (!exists()) return "";
        std::ifstream file(m_path, std::ios::in);
        if (!file.is_open()) return "";

        return std::string((std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());
    }

    // Lit le fichier ligne par ligne et retourne un tableau (vector) de strings
    std::vector<std::string> readLines() const {
        std::vector<std::string> lines;
        if (!exists()) return lines;

        std::ifstream file(m_path, std::ios::in);
        if (!file.is_open()) return lines;

        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(line);
        }
        return lines;
    }

    // Récupère uniquement le nom du fichier (ex: "data.txt")
    std::string getName() const {
        return m_path.filename().string();
    }

    // Récupère l'extension (ex: ".txt")
    std::string getExtension() const {
        return m_path.extension().string();
    }
};