#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 3) in vec2 aTexCoords;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

layout (location = 4) in mat4 aInstanceModel;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// 1 = rendu instancie (CubeRenderer : matrice par instance) ; 0 = mesh unique
// (matrice "model" en uniform, ex. les parties de la carte dans Scene).
uniform float useInstanceModel;

void main()
{
    mat4 m = useInstanceModel > 0.5 ? aInstanceModel : model;
    FragPos = vec3(m * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(m))) * aNormal;  
    TexCoords = aTexCoords;
    
    gl_Position = projection * view * vec4(FragPos, 1.0);
}