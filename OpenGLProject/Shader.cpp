#include "Shader.h"
#include "constants/window.h"
#include "Camera.h"


#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

#include <glad/glad.h>
#include <GLFW/glfw3.h>


Shader::Shader(const std::string& vertexSource, const std::string& fragmentSource, Camera* camera, bool isFile) {
    m_model = glm::mat4(1.0f);
    m_view = glm::mat4(1.0f);
    m_projection = glm::mat4(1.0f);
	m_name = std::filesystem::path(vertexSource).stem().string();
	// Résolution unique du rôle du shader : une comparaison de chaîne par création,
	// puis un simple compare d'enum (= 1 cycle CPU) à chaque draw().
	m_type = extractShaderType(m_name);
    m_camera = camera;

    m_view = m_camera->getViewMatrix();

    // Projection en perspective : effet 3D avec champ de vision de 60°
    m_projection = glm::perspective(
        glm::radians(60.0f),
        (float)Constants::Window::WINDOW_WIDTH / (float)Constants::Window::WINDOW_HEIGHT,
        0.1f, 100.0f
    );

    m_projection2D = glm::ortho(
        0.0f,
        (float)Constants::Window::WINDOW_WIDTH,
        (float)Constants::Window::WINDOW_HEIGHT,
        0.0f,
        -1.0f,
        1.0f
    );

    // Chargement du code source
    std::string vertexCode = isFile ? loadFromFile(vertexSource) : vertexSource;
    std::string fragmentCode = isFile ? loadFromFile(fragmentSource) : fragmentSource;

    // Compilation individuelle
    GLuint vertex = compile(GL_VERTEX_SHADER, vertexCode);
    GLuint fragment = compile(GL_FRAGMENT_SHADER, fragmentCode);

    // Création et linkage du programme
    m_id = glCreateProgram();
    glAttachShader(m_id, vertex);
    glAttachShader(m_id, fragment);
    glLinkProgram(m_id);

    checkCompileErrors(m_id, "PROGRAM");

    // Shaders bruts supprimés après linkage
    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

Shader::~Shader() {
    if (m_id) {
        glDeleteProgram(m_id);
    }
}

void Shader::use() {
    glUseProgram(m_id);
}

void Shader::setupMatrices() {
    m_view = m_camera->getViewMatrix();
    setMat4("model", m_model);
    setMat4("view", m_view);
    setMat4("projection", m_projection);
}

GLuint Shader::getID() const {
    return m_id;
}

std::string Shader::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void Shader::setTexture(const std::string& name, unsigned int textureID, unsigned int unit) {
    //setInt(name, unit);
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glUniform1i(getUniformLocation(name), unit);
}

unsigned int Shader::compile(unsigned int type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    checkCompileErrors(shader, (type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT"));

    return shader;
}

void Shader::checkCompileErrors(unsigned int object, std::string type) {
    GLint success;
    GLchar infoLog[1024];
    if (type == "PROGRAM") {
        glGetProgramiv(object, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(object, 1024, nullptr, infoLog);
            throw std::runtime_error("Shader linking error:\n" + std::string(infoLog));
        }
    }
    else {
        glGetShaderiv(object, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(object, 1024, nullptr, infoLog);
            throw std::runtime_error("Shader compilation error (" + type + "):\n" + std::string(infoLog));
        }
    }
}

void Shader::setBool(const std::string& name, bool value) {
    const GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform1i(location, (int)value);
    }
}

void Shader::setInt(const std::string& name, int value) {
    const GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform1i(location, value);
    }
}

void Shader::setFloat(const std::string& name, float value) {
    const GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform1f(location, value);
    }
}
void Shader::setVec2(const std::string& name, const glm::vec2& value) {
    const GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform2fv(location, 1, &value[0]);
    }
}
void Shader::setVec2(const std::string& name, float x, float y) {
    const GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform2f(location, x, y);
    }
}
void Shader::setVec3(const std::string& name, const glm::vec3& value) {
    const GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform3fv(location, 1, &value[0]);
    }
}
void Shader::setVec3(const std::string& name, float x, float y, float z) {
    const GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform3f(location, x, y, z);
    }
}
void Shader::setVec4(const std::string& name, const glm::vec4& value) {
    const GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform4fv(location, 1, &value[0]);
    }
}
void Shader::setVec4(const std::string& name, float x, float y, float z, float w) {
    const GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniform4f(location, x, y, z, w);
    }
}
void Shader::setMat2(const std::string& name, const glm::mat2& mat) {
    const GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniformMatrix2fv(location, 1, GL_FALSE, &mat[0][0]);
    }
}
void Shader::setMat3(const std::string& name, const glm::mat3& mat) {
    const GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniformMatrix3fv(location, 1, GL_FALSE, &mat[0][0]);
    }
}
void Shader::setMat4(const std::string& name, const glm::mat4& mat) {
    const GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniformMatrix4fv(location, 1, GL_FALSE, &mat[0][0]);
    }
}

void Shader::setMat4Array(const std::string& name, const glm::mat4* mats, int count) {
    if (count <= 0) return;
    GLint location = getUniformLocation(name);
    if (location != -1) {
        glUniformMatrix4fv(location, count, GL_FALSE, glm::value_ptr(mats[0]));
    }
}

void Shader::setInt(int location, int value) {
    if (location != -1) {
        glUniform1i(location, value);
    }
}

void Shader::setFloat(int location, float value) {
    if (location != -1) {
        glUniform1f(location, value);
    }
}

void Shader::setVec3(int location, const glm::vec3& value) {
    if (location != -1) {
        glUniform3fv(location, 1, &value[0]);
    }
}

void Shader::setVec4(int location, const glm::vec4& value) {
    if (location != -1) {
        glUniform4fv(location, 1, &value[0]);
    }
}

void Shader::setVec2(int location, const glm::vec2& value) {
    if (location != -1) {
        glUniform2fv(location, 1, &value[0]);
    }
}

void Shader::setMat4(int location, const glm::mat4& mat) {
    if (location != -1) {
        glUniformMatrix4fv(location, 1, GL_FALSE, &mat[0][0]);
    }
}

int Shader::getUniformLocation(const std::string& name) {
    auto it = m_uniformLocations.find(name);
    if (it != m_uniformLocations.end()) {
        return it->second; // Retourne la valeur mise en cache (valide ou -1)
    }

    GLint location = glGetUniformLocation(m_id, name.c_str());
    m_uniformLocations[name] = location; // On cache même le -1 pour éviter de re-interroger OpenGL
    return location;
}

void Shader::clearUniformLocations() {
    m_uniformLocations.clear();
}