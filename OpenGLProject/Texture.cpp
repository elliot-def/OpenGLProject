// IMPORTANT : cette macro doit rester definie dans EXACTEMENT une seule unite de compilation
// du projet entier (c'etait deja le cas ici avant la factorisation). ImageLoader.cpp et
// Image.cpp incluent stb_image.h sans cette macro : ils ne font qu'utiliser les declarations,
// l'implementation reelle (le code des fonctions stbi_*) est compilee uniquement ici.
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "Texture.h"
#include "ImageLoader.h"
#include "Shader.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <iostream>

Texture::Texture(const std::string& filePath, const int textureID, float shininess, bool hasSpecular)
    : m_filePath(filePath), m_textureID(textureID), m_shininess(shininess) {
	printf("Loading texture from: %s\n", filePath.c_str());
    m_hasSpecular = hasSpecular; // Si il trouve _specular.png dans le filename
    loadTexture(m_filePath, m_textureID);

	// Chargement de la texture spéculaire si applicable

    if (m_hasSpecular) {
		// Générer le chemin du fichier spéculaire

		m_fileSpecularPath = m_filePath;
        m_specularTextureID = textureID + 1;

        std::string ancien = ".png";
        std::string nouvelle = "_specular.png";

		// Remplacement de la partie du nom de fichier
        size_t pos = m_fileSpecularPath.find(ancien);
        if (pos != std::string::npos) { m_fileSpecularPath.replace(pos, ancien.length(), nouvelle); }

        loadTexture(m_fileSpecularPath, m_specularTextureID);
    }
}

Texture::~Texture() {
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &m_textureID);
	if (m_hasSpecular) glDeleteTextures(1, &m_specularTextureID);
}

void Texture::loadTexture(std::string& filePath, unsigned int& id) {
    // Vérifier si le fichier existe (comportement conservé : on log et on abandonne, sans exception fatale)
    if (!std::filesystem::exists(filePath)) {
        std::cerr << "File path does not exist: " << filePath << std::endl;
        return;
    }

    try {
        // wrapMode = GL_REPEAT (comportement d'origine, pour du tiling sur des materiaux 3D)
        // requestedChannels = 4 (comportement d'origine : on force toujours du RGBA)
        ImageLoader loader(filePath, /*flipVertically=*/true, /*generateMipmaps=*/true, GL_REPEAT, /*requestedChannels=*/4);

        id = loader.releaseTextureID(); // Texture reste proprietaire, ImageLoader ne la detruira plus
        m_width = loader.getWidth();
        m_height = loader.getHeight();
        m_nrChannels = 4;
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to load texture: " << filePath << " (" << e.what() << ")" << std::endl;
    }
}

void Texture::applyToShader(Shader* shader) {
    shader->setTexture("material.diffuse", m_textureID, 0);

    if (m_hasSpecular) {
        shader->setTexture("material.specular", m_specularTextureID, 1);
    }
    else {
        shader->setTexture("material.specular", m_defaultSpecularID, 1);
    }

    shader->setFloat("material.shininess", m_shininess);
}
