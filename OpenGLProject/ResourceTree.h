#pragma once
#include <algorithm>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "constants/file.h"

// Arborescence generique de ressources accedees par chemin relatif
// ("dossier/sous-dossier/nom"). Factorise TextureNode et ShaderNode, qui
// etaient deux copies quasi identiques : meme parsing stringstream + getline,
// meme deleteNode recursif, meme style de printTree.
//
// Les noeuds sont detenus par std::unique_ptr : plus aucun delete manuel, et
// aucune fuite si une exception survient pendant la construction (le
// destructeur par defaut depile tout l'arbre recursivement).
template <typename T>
class ResourceTree {
public:
    struct Node {
        // Sous-dossiers. unique_ptr => l'arbre est proprietaire et se libere
        // tout seul (recursivement) a la destruction.
        std::unordered_map<std::string, std::unique_ptr<Node>> children;
        // Ressource si c'est une feuille (Texture / Shader).
        std::unique_ptr<T> value;
    };

    // Racine de l'arborescence : les managers y inserent leurs ressources.
    Node& root() { return m_root; }
    const Node& root() const { return m_root; }

    // Cree (au besoin) les noeuds intermediaires du chemin et retourne le
    // noeud final. L'appelant y place ensuite sa ressource via `value`.
    Node& getOrCreateNode(const std::string& path) {
        Node* current = &m_root;
        std::stringstream ss(normalizePath(path));
        std::string part;
        while (std::getline(ss, part, Constants::File::PREFERED_SEPARATOR_PATH)) {
            auto& child = current->children[part];
            if (!child) child = std::make_unique<Node>();
            current = child.get();
        }
        return *current;
    }

    // Recupere une ressource par chemin relatif (ex: "dossier/sous/nom").
    // Retourne un pointeur NON possedant : l'arbre reste proprietaire.
    // @throws std::out_of_range si le chemin ou la ressource n'existe pas.
    T* find(const std::string& path) const {
        const Node* current = &m_root;
        std::stringstream ss(normalizePath(path));
        std::string part;
        while (std::getline(ss, part, Constants::File::PREFERED_SEPARATOR_PATH)) {
            auto it = current->children.find(part);
            if (it == current->children.end()) {
                throw std::out_of_range("Path not found: " + path);
            }
            current = it->second.get();
        }
        if (!current->value) {
            throw std::out_of_range("No resource at path: " + path);
        }
        return current->value.get();
    }

private:
    // Normalise les separateurs en '/' : les chemins natifs Windows
    // (std::filesystem::path::string()) utilisent '\\', tandis que les
    // chemins d'acces fournis par les appelants utilisent '/'. Une seule
    // convention evite le bug ou insertion ('\\' sur Windows) et lecture ('/')
    // ne produisaient pas les memes segments.
    static std::string normalizePath(std::string path) {
        std::replace(path.begin(), path.end(), '\\', '/');
        return path;
    }

    Node m_root;
};
