#pragma once
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <unordered_map>

#include "Transformation.h"
#include "Camera.h"
#include "ShaderType.h"

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

    // Vide le cache des uniforms
    void clearUniformLocations();

    // Retourne l'identifiant OpenGL du programme shader
    unsigned int getID() const;

	// Envoie les matrices model, view, projection au shader
    void setupMatrices();

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

    // Envoie un tableau de mat4 (utilise pour uBoneMatrices du skinning).
    // count = nombre de matrices ; count > 0 requis.
    void setMat4Array(const std::string& name, const glm::mat4* mats, int count);

    // Variantes "location directe" (hot path) : l'appelant fournit une GLint
    // deja resolue une fois par shader (ex: cache dans LightManager/Spotlight)
    // pour bypasser completement le hash map name->location a chaque frame.
    // Location == -1 => no-op silencieux (meme comportement que par nom).
    void setInt(int location, int value);
    void setFloat(int location, float value);
    void setVec3(int location, const glm::vec3& value);
    void setVec4(int location, const glm::vec4& value);
    void setVec2(int location, const glm::vec2& value);
    void setMat4(int location, const glm::mat4& mat);

    // Resout (avec cache) la location d'un uniform. Retourne -1 si absent.
    // Public pour les caches par shader (LightManager, Spotlight...).
    int getUniformLocation(const std::string& name);

    // Méthodes pour modifier les matrices internes du shader
    void setModel(const glm::mat4& model) { m_model = model; }
    void setView(const glm::mat4& view) { m_view = view; }
    void setProjection(const glm::mat4& projection) { m_projection = projection; }

    // Méthodes pour obtenir les matrices internes
    const glm::mat4& getModel() const { return m_model; }
    const glm::mat4& getView() const { return m_view; }
    const glm::mat4& getProjection() const { return m_projection; }
    const glm::mat4& getProjection2D() const { return m_projection2D; }
	inline const Camera* getCamera() const { return m_camera; }
	inline const std::string getName() const { return m_name; }
	inline ShaderType getType() const { return m_type; }
private:
    unsigned int m_id;  // ID OpenGL du programme shader
    Camera* m_camera;  // Pointeur vers la caméra pour récupérer la vue
	std::string m_name; // Noms ou sources des shaders
	ShaderType m_type = ShaderType::Unknown; // Role du shader, resolu une fois dans le constructeur
    glm::mat4 m_projection, m_projection2D, m_model, m_view = glm::mat4(1.0f);  // Matrices de transformation
    std::unordered_map<std::string, int> m_uniformLocations; // Cache des emplacements des uniforms

    // Charge le code source depuis un fichier
    std::string loadFromFile(const std::string& path);

    // Compile un shader GLSL (vertex ou fragment)
    unsigned int compile(unsigned int type, const std::string& source);

    // Vérifie les erreurs de compilation ou de linkage
    void checkCompileErrors(unsigned int shader, std::string type);
};
