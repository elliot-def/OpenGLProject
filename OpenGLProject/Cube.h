#pragma once

#include <glm/glm.hpp>
#include <vector>

#include "Vertex.h"
#include "Outlineable.h"

class Shader;
class Mesh;
class Shader;
class Texture;
class Transformation;
class LightManager;
class LightSource;
class Player;
class Renderer;

// Classe Cube : represente un cube 3D dans le jeu
class Cube : public Outlineable {
public:
    // Constructeur
    // center : position du centre du cube
    // edge : taille d'une ar�te du cube
    // shader : shader utilis� pour le rendu
    // texture : texture appliqu�e au cube
    
    Cube(glm::vec3 center, float edge, Shader* shader, LightSource* lightSource, Player* player);
    Cube(glm::vec3 center, float edge, Shader* shader, std::vector<Texture*> textures, Renderer* renderer, LightManager* lightManager, Player* player);

    // Destructeur : libere la memoire (mesh, transformation�)
    ~Cube();

    // Mise � jour du cube (transformations, animations, effets�)
    void update();

    // Dessine le cube a l'ecran (appelle Mesh + Shader)
    virtual void draw();

	inline Transformation* getTransformation() const { return m_transformation; }

    // Retourne la texture du cube
    inline std::vector<Texture*> getTextures() const;

	inline glm::vec3 getCenter() const { return m_center; }

	inline Mesh* getMesh() const { return m_mesh; }

    // Outline (silhouette) — voir Outlineable.h
    void setOutlineShader(Shader* s) { m_outlineShader = s; }
    Shader* getOutlineShader() const { return m_outlineShader; }

protected:
    Cube(glm::vec3 center, float edge, Shader* shader, Player* player);

    Mesh* m_mesh = nullptr;           // Maillage du cube (buffers OpenGL)
    glm::vec3 m_center;               // Centre du cube dans l'espace
    std::vector<Texture*> m_textures; // Texture appliquee
    Shader* m_shader;                 // Shader pour le rendu
    Shader* m_outlineShader = nullptr;  // Outline (silhouette)
    Transformation* m_transformation = nullptr; // Transformations : position, rotation, scale
	LightManager* m_lightManager;     // Pointeur vers le LightBlock associ� (si applicable)
	LightSource* m_lightSource;       // Pointeur vers le LightSource associ� (si applicable)
    Player* m_player;
    Renderer* m_renderer;

    float m_edge;                      // Taille d'une ar�te du cube

private:
    void drawLightSourceShader();
    void drawSeveralLightShader();
};