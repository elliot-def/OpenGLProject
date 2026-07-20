#include "ImageLoader.h"

#include <stb/stb_image.h>
#include <stdexcept>
#include <iostream>

ImageLoader::ImageLoader(const std::string& imagePath, bool flipVertically, bool generateMipmaps, GLenum wrapMode, int requestedChannels) {
    load(imagePath, flipVertically, generateMipmaps, wrapMode, requestedChannels);
}

ImageLoader::~ImageLoader() {
    release();
}

ImageLoader::ImageLoader(ImageLoader&& other) noexcept
    : m_textureID(other.m_textureID), m_width(other.m_width),
      m_height(other.m_height), m_channels(other.m_channels) {
    other.m_textureID = 0;
}

ImageLoader& ImageLoader::operator=(ImageLoader&& other) noexcept {
    if (this != &other) {
        release();
        m_textureID = other.m_textureID;
        m_width = other.m_width;
        m_height = other.m_height;
        m_channels = other.m_channels;
        other.m_textureID = 0;
    }
    return *this;
}

void ImageLoader::load(const std::string& imagePath, bool flipVertically, bool generateMipmaps, GLenum wrapMode, int requestedChannels) {
    // stb_image lit le pixel (0,0) en haut a gauche par defaut.
    // OpenGL attend l'origine UV (0,0) en bas a gauche : on flip donc verticalement
    // pour que la texture s'affiche dans le bon sens, sans avoir a inverser les UV a la main.
    stbi_set_flip_vertically_on_load(flipVertically);

    unsigned char* data = stbi_load(imagePath.c_str(), &m_width, &m_height, &m_channels, requestedChannels);
    if (!data) {
        throw std::runtime_error("ImageLoader : impossible de charger \"" + imagePath + "\" : " + stbi_failure_reason());
    }

    // Si on force un nombre de canaux (ex: 4 pour du RGBA), c'est CE nombre qui determine
    // le format d'upload GPU, pas celui detecte dans le fichier source (m_channels reste informatif).
    int channelsForFormat = (requestedChannels != 0) ? requestedChannels : m_channels;

    GLenum format = (channelsForFormat == 4) ? GL_RGBA
                  : (channelsForFormat == 3) ? GL_RGB
                  : (channelsForFormat == 2) ? GL_RG
                  : GL_RED;

    glGenTextures(1, &m_textureID);
    glBindTexture(GL_TEXTURE_2D, m_textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, generateMipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, format, m_width, m_height, 0, format, GL_UNSIGNED_BYTE, data);
    if (generateMipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    std::cout << "[ImageLoader] Texture chargee : " << imagePath
        << " (" << m_width << "x" << m_height << ", " << m_channels << " canaux, ID=" << m_textureID << ")" << std::endl;
}

void ImageLoader::release() {
    if (m_textureID != 0) {
        glDeleteTextures(1, &m_textureID);
        m_textureID = 0;
    }
}
