#pragma once
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <unordered_map>

#include "Transformation.h"
#include "Camera.h"

// Classe qui encapsule un shader OpenGL (vertex + fragment)
// Gère la compilation, le linkage, l'utilisation et l'envoi des variables uniformes
class Shader {
public:
    // Constructeur : compile et lie les shaders
    // vertexSource et fragmentSource peuvent être des chemins de fichiers ou du code source directement
    Shader(const std::string& vertexSource, const std::string& fragmentSource, Camera* camera, bool isFile);

    // Destructeur : supprime le programme shader côté GPU
    ~Shader();

    // Active ce shader pour le rendu et envoie les matrices model, view, projection
    void use();

    // Associe une texture 2D à un uniform du shader
    void setTexture(const std::string& name, unsigned int textureID, unsigned int unit);

    // Envoie une transformation encapsulée dans la classe Transformation
    void setTransformation(const std::string& name, Transformation* trans);

    // Vide le cache des uniforms
    void clearUniformLocations();

    // Retourne l'identifiant OpenGL du programme shader
    unsigned int getID() const;

	// Envoie les matrices model, view, projection au shader
    void setupMatrices();
    void setupMatrices2D();

    // Méthodes pour envoyer des variables uniformes simples
    void setBool(const std::string& name, bool value);
    void setInt(const std::string& name, int value);
    void setFloat(const std::string& name, float value);
    void setVec2(const std::string& name, const glm::vec2& value);
    void setVec2(const std::string& name, float x, float y);
    void setVec3(const std::string& name, const glm::vec3& value);
    void setVec3(const std::string& name, float x, float y, float z);
    void setVec4(const std::string& name, const glm::vec4& value);
    void setVec4(const std::string& name, float x, float y, float z, float w);
    void setMat2(const std::string& name, const glm::mat2& mat);
    void setMat3(const std::string& name, const glm::mat3& mat);
    void setMat4(const std::string& name, const glm::mat4& mat);

    // Méthodes pour modifier les matrices internes du shader
    void setModel(const glm::mat4& model) { m_model = model; }
    void setView(const glm::mat4& view) { m_view = view; }
    void setProjection(const glm::mat4& projection) { m_projection = projection; }

    // Méthodes pour obtenir les matrices internes
    const glm::mat4& getModel() const { return m_model; }
    const glm::mat4& getView() const { return m_view; }
    const glm::mat4& getProjection() const { return m_projection; }
	inline const Camera* getCamera() const { return m_camera; }
	inline const std::string getName() const { return m_name; }
private:
    unsigned int m_id;  // ID OpenGL du programme shader
    Camera* m_camera;  // Pointeur vers la caméra pour récupérer la vue
	std::string m_name; // Noms ou sources des shaders
    glm::mat4 m_projection, m_projection2D, m_model, m_view = glm::mat4(1.0f);  // Matrices de transformation
    std::unordered_map<std::string, int> m_uniformLocations; // Cache des emplacements des uniforms

    // Charge le code source depuis un fichier
    std::string loadFromFile(const std::string& path);

    // Compile un shader GLSL (vertex ou fragment)
    unsigned int compile(unsigned int type, const std::string& source);

    // Vérifie les erreurs de compilation ou de linkage
    void checkCompileErrors(unsigned int shader, std::string type);

    // Récupère l'emplacement d'un uniform dans le shader (avec cache)
    int getUniformLocation(const std::string& name);

	
};
