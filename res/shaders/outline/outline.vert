#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main() {
    // L'expansion de l'outline est gérée côté C++ en multipliant uModel par
    // glm::scale(1 + thickness). Le shader ne fait que forwarder la position
    // transformée — l'épaisseur reste uniforme quelle que soit la distance
    // caméra puisque c'est un scale en object space.
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
}
