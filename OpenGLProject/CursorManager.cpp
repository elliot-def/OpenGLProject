#include "CursorManager.h"

#include "Window.h"

#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb/stb_image.h>

void CursorManager::setCustomCursor(const char* cursorPath) {
    int srcWidth, srcHeight, srcChannels;
    unsigned char* cursor_pixels = stbi_load(cursorPath, &srcWidth, &srcHeight, &srcChannels, 4);

    if (cursor_pixels) {
        // 1. Calcul des dimensions cibles
        int destWidth = static_cast<int>(srcWidth * m_size);
        int destHeight = static_cast<int>(srcHeight * m_size);

        // 2. Allocation du tableau à la vraie taille requise
        std::vector<unsigned char> scaledFlippedPixels(destWidth * destHeight * 4, 0);

        // 3. Remplissage avec redimensionnement et Flip vertical simultanés
        for (int y = 0; y < destHeight; ++y) {
            for (int x = 0; x < destWidth; ++x) {

                // Retrouver la coordonnée source correspondante
                int srcX = static_cast<int>(x / m_size);
                int srcY = static_cast<int>(y / m_size);

                // Sécurité pour les arrondis aux bordures
                if (srcX >= srcWidth)  srcX = srcWidth - 1;
                if (srcY >= srcHeight) srcY = srcHeight - 1;

                // Application du Flip Vertical (destHeight - 1 - y)
                int destIdx = ((destHeight - 1 - y) * destWidth + x) * 4;
                int srcIdx = (srcY * srcWidth + srcX) * 4;

                scaledFlippedPixels[destIdx] = cursor_pixels[srcIdx];
                scaledFlippedPixels[destIdx + 1] = cursor_pixels[srcIdx + 1];
                scaledFlippedPixels[destIdx + 2] = cursor_pixels[srcIdx + 2];
                scaledFlippedPixels[destIdx + 3] = cursor_pixels[srcIdx + 3];
            }
        }

        GLFWimage cursor_image{};
        cursor_image.width = destWidth;
        cursor_image.height = destHeight;
        cursor_image.pixels = scaledFlippedPixels.data();

        if (m_GLFWcursor) {
            glfwDestroyCursor(m_GLFWcursor);
        }

        // Hotspot à (0,0) pour les pointeurs standard. 
        // Si vous voulez qu'un viseur (crosshair) reste centré, mettez (destWidth/2, destHeight/2)
        m_GLFWcursor = glfwCreateCursor(&cursor_image, 0, 0);

        if (m_GLFWcursor) {
            glfwSetCursor(m_window->getGLFWwindow(), m_GLFWcursor);
        }
        else {
            std::cerr << "Erreur: Impossible de créer le curseur GLFW" << std::endl;
        }

        stbi_image_free(cursor_pixels);
    }
    else {
        std::cerr << "Erreur: Impossible de charger l'image du curseur " << cursorPath << std::endl;
    }
}

void CursorManager::updateCursor() {
    // Si on est en mode "wait", on met à jour la frame selon le temps
    if (m_currentCursor.first == "wait" && !m_waitCursorFrames.empty()) {

        // 1. Calculer la frame actuelle en fonction du temps
        double currentTime = glfwGetTime();
        float animationSpeed = 10.0f; // Nombre de frames par seconde

        int totalFrames = static_cast<int>(m_waitCursorFrames.size());
        int frameIndex = static_cast<int>(currentTime * animationSpeed) % totalFrames;

        // 2. Appliquer la frame si elle a changé
        if (frameIndex != m_currentFrameIndex) {
            m_currentFrameIndex = frameIndex;

            // On applique directement le curseur GLFW pré-chargé à votre fenêtre
            glfwSetCursor(m_window->getGLFWwindow(), m_waitCursorFrames[m_currentFrameIndex]);
        }
    }
}

void CursorManager::loadWaitFrames() {
    int width = 0, height = 0, channels = 0;

    // 1. Charger l'image de base en RGBA (4 canaux obligatoire pour GLFW)
    unsigned char* basePixels = stbi_load(m_cursorsAvailable.at("wait").c_str(), &width, &height, &channels, 4);

    if (!basePixels) {
        std::cerr << "Erreur : Impossible de charger l'image du curseur wait." << std::endl;
        return;
    }

    // Le centre de rotation du curseur
    float cx = width / 2.0f;
    float cy = height / 2.0f;
    const float PI = 3.1415926535f;

    // 2. Générer les 8 frames de rotation
    for (int i = 0; i < 8; ++i) {
        // Calcul de l'angle pour cette frame (i * 45 degrés en radians)
        float angle = i * (2.0f * PI / 8.0f);
        float cosA = std::cos(-angle); // Angle inversé pour la rotation des coordonnées
        float sinA = std::sin(-angle);

        // On crée un tableau temporaire pour stocker les pixels de la nouvelle image copiée/tournée
        // Initialisé à 0 (transparent)
        std::vector<unsigned char> rotatedPixels(width * height * 4, 0);

        // Algorithme de rotation pixel par pixel (Nearest Neighbor)
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                // Coordonnées relatives au centre du curseur
                float nx = x - cx + 0.5f;
                float ny = y - cy + 0.5f;

                // On cherche d'où vient ce pixel dans l'image d'origine (rotation inverse)
                int srcX = static_cast<int>(nx * cosA - ny * sinA + cx);
                int srcY = static_cast<int>(nx * sinA + ny * cosA + cy);

                // Si le pixel d'origine est bien dans les limites de l'image, on le copie
                if (srcX >= 0 && srcX < width && srcY >= 0 && srcY < height) {
                    int destIdx = (y * width + x) * 4;
                    int srcIdx = (srcY * width + srcX) * 4;

                    rotatedPixels[destIdx] = basePixels[srcIdx];     // R
                    rotatedPixels[destIdx + 1] = basePixels[srcIdx + 1]; // G
                    rotatedPixels[destIdx + 2] = basePixels[srcIdx + 2]; // B
                    rotatedPixels[destIdx + 3] = basePixels[srcIdx + 3]; // A
                }
            }
        }

        // 3. Convertir notre vecteur de pixels tournés en structure GLFWimage
        GLFWimage glfwImage;
        glfwImage.width = width;
        glfwImage.height = height;
        glfwImage.pixels = rotatedPixels.data();

        // 4. Créer le curseur GLFW
        // Note : Pour un loader qui tourne, le "Hotspot" (point d'ancrage du clic) 
        // doit idéalement être placé au centre (cx, cy) pour qu'il pivote sur lui-même correctement.
        GLFWcursor* cursorFrame = glfwCreateCursor(&glfwImage, static_cast<int>(cx), static_cast<int>(cy));

        if (cursorFrame) {
            m_waitCursorFrames.push_back(cursorFrame);
        }
    }

    // 5. Ne pas oublier de libérer la mémoire de l'image d'origine
    stbi_image_free(basePixels);
}