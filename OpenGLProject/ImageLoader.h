#pragma once

#include <string>
#include <glad/glad.h>

// Classe ImageLoader : charge une image depuis le disque en tant que texture OpenGL
//
// Contrairement a la classe "Image" (qui herite de Shape et sert a l'affichage 2D),
// ImageLoader est une classe utilitaire independante : elle ne fait que charger
// le fichier, creer la texture GPU et gerer automatiquement le bon sens (flip),
// puis expose l'ID de texture + les infos (largeur, hauteur, canaux).
//
// Sert aussi de brique commune pour factoriser le chargement stb_image qui etait
// duplique entre Image::loadTexture() et Texture::loadTexture().
//
// Utilisation :
// @code
// ImageLoader img("./res/textures/herbe.png");
// glBindTexture(GL_TEXTURE_2D, img.getTextureID());
//
// // Pour transferer la texture a une classe qui gere elle-meme sa duree de vie (ex: Texture) :
// unsigned int id = img.releaseTextureID(); // img ne la detruira plus dans son destructeur
// @endcode
class ImageLoader {
public:
    // imagePath        : chemin vers le fichier image (png, jpg, etc.)
    // flipVertically   : true par defaut car OpenGL attend l'origine UV en bas a gauche,
    //                    alors que la plupart des formats image stockent le pixel (0,0) en haut a gauche.
    // generateMipmaps  : genere les mipmaps automatiquement (utile pour les textures 3D, inutile pour de l'UI 2D)
    // wrapMode         : GL_REPEAT pour une texture qui doit boucler (materiaux 3D, sols, murs...),
    //                    GL_CLAMP_TO_EDGE pour un sprite/UI qui ne doit pas boucler sur les bords.
    // requestedChannels: 0 = detection automatique du nombre de canaux de l'image.
    //                    Sinon force stb_image a produire ce nombre de canaux (ex: 4 pour forcer du RGBA,
    //                    pratique quand le shader s'attend toujours au meme format quelle que soit l'image source).
    explicit ImageLoader(const std::string& imagePath,
        bool flipVertically = true,
        bool generateMipmaps = true,
        GLenum wrapMode = GL_REPEAT,
        int requestedChannels = 0);

    ~ImageLoader();

    // Pas de copie (on possede une ressource GPU unique)
    ImageLoader(const ImageLoader&) = delete;
    ImageLoader& operator=(const ImageLoader&) = delete;

    // Deplacement autorise (transfert de la propriete de la texture)
    ImageLoader(ImageLoader&& other) noexcept;
    ImageLoader& operator=(ImageLoader&& other) noexcept;

    unsigned int getTextureID() const { return m_textureID; }
    int getWidth()    const { return m_width; }
    int getHeight()   const { return m_height; }
    int getChannels() const { return m_channels; }
    bool isValid()    const { return m_textureID != 0; }

    // Transfere la propriete de la texture GPU a l'appelant : l'ID est retourne
    // et ImageLoader ne le detruira plus dans son destructeur.
    // A utiliser quand une autre classe (ex: Texture) veut gerer elle-meme la duree de vie de la texture.
    unsigned int releaseTextureID() {
        unsigned int id = m_textureID;
        m_textureID = 0;
        return id;
    }

private:
    void load(const std::string& imagePath, bool flipVertically, bool generateMipmaps, GLenum wrapMode, int requestedChannels);
    void release();

    unsigned int m_textureID = 0;
    int m_width = 0;
    int m_height = 0;
    int m_channels = 0;
};
