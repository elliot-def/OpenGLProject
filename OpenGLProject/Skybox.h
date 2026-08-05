#pragma once

#include <string>
#include <glm/glm.hpp>

class Shader;
class Camera;

// Classe Skybox : ciel procédural/image autour de la scène.
//
// Charge un cubemap OpenGL depuis un dossier contenant les 6 faces nommées
// px.png / nx.png / py.png / ny.png / pz.png / nz.png (convention standard),
// puis le dessine comme un cube géant centré sur la caméra : la vue est
// débarrassée de sa translation, donc la skybox « suit » toujours le joueur.
//
// Utilisation :
//   Skybox sky("./res/skybox/night/night");
//   sky.draw(m_shaderManager->getShader("skybox"), m_camera.get());
class Skybox {
public:
    // facesFolder : dossier contenant px.png, nx.png, py.png, ny.png, pz.png, nz.png
    explicit Skybox(const std::string& facesFolder);

    ~Skybox();

    // Pas de copie (ressource GPU unique)
    Skybox(const Skybox&) = delete;
    Skybox& operator=(const Skybox&) = delete;

    // Dessine le ciel. À appeler en début de frame, avant les objets opaques :
    // le cube est rendu avec glDepthFunc(GL_LEQUAL) + depth mask OFF pour que
    // la géométrie de la scène (plus proche) le recouvre naturellement.
    void draw(Shader* shader, Camera* camera);

private:
    // Charge les 6 faces et crée la texture cubemap GPU
    unsigned int loadCubemap(const std::string& facesFolder);

    // Construit le VAO/VBO du cube unité (-1..1)
    void setupMesh();

    unsigned int m_cubemapID = 0;
    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;
    std::string m_facesFolder;
};
