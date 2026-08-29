// NOTE : STB_IMAGE_IMPLEMENTATION est défini UNIQUEMENT dans Texture.cpp
// (une seule unité de compilation du projet). Ici on utilise stb_image en
// lecture seule, comme Image.cpp / ImageLoader.cpp.
#include <stb/stb_image.h>

#include "Log.h"
#include "Skybox.h"
#include "Shader.h"
#include "Camera.h"

#include <glad/glad.h>
#include <iostream>
#include <filesystem>

Skybox::Skybox(const std::string& facesFolder)
    : m_facesFolder(facesFolder) {
    m_cubemapID = loadCubemap(facesFolder);
    setupMesh();
    if (m_cubemapID) {
        logOut() << "[Skybox] Cubemap charge depuis : " << facesFolder << std::endl;
    }
}

Skybox::~Skybox() {
    if (m_vao)  glDeleteVertexArrays(1, &m_vao);
    if (m_vbo)  glDeleteBuffers(1, &m_vbo);
    if (m_cubemapID) glDeleteTextures(1, &m_cubemapID);
}

unsigned int Skybox::loadCubemap(const std::string& facesFolder) {
    // Convention standard des exports skybox : px, nx, py, ny, pz, nz.
    static const char* SUFFIXES[] = { "px", "nx", "py", "ny", "pz", "nz" };
    static const GLenum TARGETS[] = {
        GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
    };

    unsigned int id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, id);

    unsigned int loadedFaces = 0;
    for (int i = 0; i < 6; i++) {
        std::string path = facesFolder + "/" + SUFFIXES[i] + ".png";
        if (!std::filesystem::exists(path)) {
            logErr() << "[Skybox] Fichier de face introuvable : " << path << std::endl;
            continue;
        }

        int w = 0, h = 0, nrChannels = 0;
        // Pas de flip vertical : les faces de cubemap suivent la convention
        // OpenGL (origin en bas à gauche) telle qu'exportée par les outils
        // générateurs de skybox. Le flip global est réglé explicitement car
        // ImageLoader/Texture le laissent à true après leur chargement.
        stbi_set_flip_vertically_on_load(false);
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &nrChannels, 0);
        if (!data) {
            logErr() << "[Skybox] Echec du chargement de la face : " << path
                      << " (" << stbi_failure_reason() << ")" << std::endl;
            continue;
        }

        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(TARGETS[i], 0, static_cast<GLint>(format), w, h, 0,
                     format, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
        loadedFaces++;
    }

    if (loadedFaces < 6) {
        // Cubemap incomplet : les faces manquantes contiennent des donnees
        // indefinies → echantillonnage noir. On signale clairement plutot
        // que de rendre un ciel corrompu silencieusement.
        logErr() << "[Skybox] ATTENTION : cubemap incomplet (" << loadedFaces
                  << "/6 faces chargees) — le ciel peut apparaitre noir."
                  << std::endl;
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    return id;
}

void Skybox::setupMesh() {
    // Cube unité (36 sommets, positions seules) : le shader échantillonne le
    // cubemap avec la position locale, donc pas besoin de UV ni de normales.
    static const float VERTICES[] = {
        // +X
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
        // -X
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,
        // +Y
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        // -Y
        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,
        // +Z
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        // -Z
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(VERTICES), VERTICES, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glBindVertexArray(0);
}

void Skybox::draw(Shader* shader, Camera* camera) {
    if (!m_cubemapID || !shader || !camera) return;

    // Le cube est rendu "derrière" toute la géométrie :
    //  - depth mask OFF : la skybox n'écrit pas dans le depth buffer ;
    //  - glDepthFunc(GL_LEQUAL) : elle ne passe que là où rien n'est plus près.
    // Le culling reste tel quel (désactivé par défaut dans ce projet) : on voit
    // l'intérieur du cube.
    // On active le depth test explicitement : le rendu skybox doit être
    // autonome quel que soit l'état GL laissé par la frame précédente.
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);

    shader->use();

    // View SANS translation : la skybox est centrée sur la caméra en
    // permanence (les positions locales du cube sont directement utilisées
    // comme direction d'échantillonnage du cubemap).
    const glm::mat4 view = glm::mat4(glm::mat3(camera->getViewMatrix()));
    shader->setMat4("model", glm::mat4(1.0f));
    shader->setMat4("view", view);
    shader->setMat4("projection", shader->getProjection());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_cubemapID);
    shader->setInt("skybox", 0);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    // Restaurer l'état pour le reste de la frame
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
}
