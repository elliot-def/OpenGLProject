#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>
#include <vector>

class Mesh;
class Shader;
class Texture;
class Camera;
class LightManager;

// Donnée d'une instance de cube : mat4 model (translate·spin·scale) + couleur
// (utilisée par le shader lightsource ; inutilisée pour les cubes texturés).
struct CubeInstance {
    glm::mat4 model = glm::mat4(1.0f);
    glm::vec4 color = glm::vec4(1.0f);
};
static_assert(sizeof(CubeInstance) == 80, "CubeInstance = mat4 (64) + vec4 (16) = 80 octets");

// ─────────────────────────────────────────────────────────────────────────────
// CubeRenderer : rendu INSTANCIÉ des cubes.
//
// Possède UN cube unitaire partagé (edge=1, centre origine, AABB locale ±0.5)
// et regroupe les instances par shader. Au draw(), chaque lot est dessiné en
// UN SEUL glDrawElementsInstanced (view/projection/éclairage appliqués une
// seule fois par shader), au lieu d'un draw call + set d'uniforms par cube.
// ─────────────────────────────────────────────────────────────────────────────
class CubeRenderer {
public:
    CubeRenderer();
    ~CubeRenderer();

    // Mesh unitaire partagé. Réutilisé par le CollisionManager (AABB locale
    // ±0.5 × matrice modèle = AABB world, identique à l'ancien Cube).
    Mesh* getUnitCubeMesh() const { return m_unitCube.get(); }

    // Vide les lots (à appeler en début de passe).
    void clear();

    // Ajoute une instance au lot du shader donné. `textures` = matériau du lot
    // (vide pour un cube de lumière). `color` = couleur par instance (lightsource).
    void submit(Shader* shader, const std::vector<Texture*>& textures,
                const glm::mat4& model, const glm::vec3& color = glm::vec3(1.0f));

    // Dessine tous les lots.
    void draw(Camera* camera, LightManager* lightManager);

private:
    struct Batch {
        std::vector<Texture*> textures;   // matériau du lot (SeveralLights)
        std::vector<CubeInstance> instances;
    };

    std::unique_ptr<Mesh> m_unitCube;
    std::unordered_map<Shader*, Batch> m_batches;
};
