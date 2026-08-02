#pragma once
#include <string>
#include <iostream>
#include <filesystem>

#include "constants/texture.h"

class Shader;

// Classe Texture : représente une texture chargée sur le GPU OpenGL
class Texture {
public:
    /**
     * @brief Constructeur : charge la texture depuis un fichier
     * @param filePath Chemin du fichier image
     * @param textureID ID OpenGL à assigner à la texture
	 * @param shininess Brillance de la surface
	 * @param hasSpecular Indique si la texture a une carte spéculaire
     */
    Texture(const std::string& filePath, const int textureID, float shininess, bool hasSpecular);

    /**
     * @brief Destructeur : libère la texture côté GPU
     */
    ~Texture();

    // Accesseurs pour les propriétés de la texture
    inline int getWidth() const { return m_width; }          // Largeur en pixels
    inline int getHeight() const { return m_height; }        // Hauteur en pixels
    inline unsigned int getID() const { return m_textureID; } // ID OpenGL
	inline bool hasSpecular() const { return m_hasSpecular; } // Indique si la texture a une carte spéculaire
	inline float getShininess() const { return m_shininess; } // Retourne la brillance de la surface
    void applyToShader(Shader* shader);


private:
    /**
     * @brief Charge la texture depuis le fichier et l'envoie sur le GPU
     */
    void loadTexture(std::string& filePath, unsigned int& id);

    std::string m_filePath;                                             // Chemin du fichier source de la texture
    std::string m_fileSpecularPath;                                     // Chemin du fichier source de la texture
    unsigned int m_textureID;                                           // ID OpenGL de la texture
    unsigned int m_specularTextureID;                                   // ID OpenGL de la texture
	unsigned int m_defaultSpecularID = Constants::Texture::BLACK_TEXTURE_ID;     // ID OpenGL de la texture spéculaire par défaut

    int m_width, m_height;         // Dimensions de la texture
    int m_nrChannels;              // Nombre de canaux (ex: 3 pour RGB, 4 pour RGBA)
    float m_shininess;
    bool m_hasSpecular;
};
