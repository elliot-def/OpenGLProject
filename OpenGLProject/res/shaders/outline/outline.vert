#version 330 core

// Outline (silhouette) : on ne lit QUE la position (attrib 0 = aPos).
// Les autres attribs du VAO (normal/color/uv/bones) sont actives par les
// differents meshes mais ignorees par le shader -> OpenGL les consomme sans
// effet (pas de lecture dans main()), donc un seul vert/frag marche pour
// Cube, ModelEntity, Triangle, Rectangle, Image, MaskImage.
//
// Effet visuel produit cote C++ :
//   - uModel = scale(1 + outlineThickness) * transformation -> elargit la
//     silhouette autour de l'origine locale.
//   - glDepthMask(GL_FALSE) avant le draw -> l'outline ne touche pas le
//     depth buffer, donc il est visible derriere le rendu principal quand
//     le scale-up cree des pixels non couverts par le rendu normal.
layout (location = 0) in vec3 aPos;

// 3 matrices uniformes, meme nommage que les autres shaders 3D
// (cube/severallights, skinned, etc.) pour partager Shader::setupMatrices().
// Pour le path 2D (Triangle, Rectangle), Shader::setupMatrices2D() envoie
// maintenant uProjection = m_projection2D en plus de projection (alias).
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main() {
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
}
