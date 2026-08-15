#include "LoadingScreen.h"
#include "Log.h"
#include "TextRenderer.h"
#include "constants/window.h"

#include <glad/glad.h>
#include <algorithm>
#include <cstdio>

// ---------------------------------------------------------------------------
// Noms des étapes de chargement
// ---------------------------------------------------------------------------

static const char* STEP_NAMES[] = {
    "Initialisation...",          // 1 (systèmes + polices)
    "Reseau...",                  // 2
    "Textures & Lumieres...",     // 3
    "Décors & Collisions...",     // 4
    "Steam...",                   // 5
    "Modeles 3D...",              // 6
    "Finalisation..."             // 7
};

const char* LoadingScreen::getStepName(int index) {
    if (index < 1 || index > STEP_COUNT) return nullptr;
    return STEP_NAMES[index - 1];
}

// ---------------------------------------------------------------------------
// Shaders inline (pas de dépendance à ShaderManager)
// ---------------------------------------------------------------------------

static const char* VERTEX_SRC = R"(#version 330 core
out vec2 vUV;
void main() {
    float x = float((gl_VertexID & 1) << 2) - 1.0;
    float y = float((gl_VertexID & 2) << 1) - 1.0;
    vUV = vec2((x + 1.0) * 0.5, (y + 1.0) * 0.5);
    gl_Position = vec4(x, y, 0.0, 1.0);
})";

static const char* FRAGMENT_SRC = R"(#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform float uTime;
uniform float uAlpha;
uniform float uProgress;   // 0..1, progression par étapes (lerp CPU)
uniform int   uStep;
uniform int   uStepCount;

void main() {
    vec3 bg     = vec3(35.0/255.0, 31.0/255.0, 32.0/255.0);   // #231F20
    vec3 accent = vec3(187.0/255.0, 68.0/255.0, 48.0/255.0);  // #BB4430

    // ── Barre de progression (22px, nette) ──
    float barH   = 0.020;
    float edge   = 0.0015;
    float barY   = abs(vUV.y - 0.42);
    float barAlpha = smoothstep(0.0, edge, barH - barY);

    float barW = 0.35;
    float barL = 0.5 - barW * 0.5;

    float inBar = smoothstep(barL - 0.002, barL, vUV.x)
                * (1.0 - smoothstep(barL + barW * uProgress - 0.002,
                                    barL + barW * uProgress, vUV.x));

    // ── Point lumineux pulsé au centre ──
    vec2 center = vUV - 0.5;
    float pulse = sin(uTime * 2.5) * 0.5 + 0.5;
    float glow  = exp(-length(center) * 18.0) * (0.06 + pulse * 0.05);

    // ── Points d'étape sous la barre ──
    float dots = 0.0;
    if (uStepCount > 0) {
        float dotR    = 0.006;
        float dotY    = 0.385;
        float totalW  = float(uStepCount - 1) * 0.028;
        float startX  = 0.5 - totalW * 0.5;
        for (int i = 1; i <= uStepCount; i++) {
            float dx = vUV.x - (startX + float(i - 1) * 0.028);
            float dy = vUV.y - dotY;
            float d  = sqrt(dx * dx + dy * dy);
            float dotAlpha = 1.0 - smoothstep(dotR - 0.001, dotR + 0.001, d);
            if (i <= uStep) {
                dots += dotAlpha;
            } else {
                dots += dotAlpha * 0.25;
            }
        }
    }

    vec3 color = bg + accent * (barAlpha * inBar * 0.80 + glow + dots * 0.70);
    FragColor = vec4(color, uAlpha);
})";

// ---------------------------------------------------------------------------
// Compilation du shader
// ---------------------------------------------------------------------------

static unsigned int compileProgram() {
    auto compile = [](GLenum type, const char* src) -> unsigned int {
        unsigned int s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        int ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512];
            glGetShaderInfoLog(s, 511, nullptr, log);
            logPrintf("[LoadingScreen] ERREUR compile shader: %s\n", log);
        }
        return s;
    };

    unsigned int vs = compile(GL_VERTEX_SHADER,   VERTEX_SRC);
    unsigned int fs = compile(GL_FRAGMENT_SHADER, FRAGMENT_SRC);

    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    int ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, 511, nullptr, log);
        logPrintf("[LoadingScreen] ERREUR link: %s\n", log);
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// ---------------------------------------------------------------------------
// Constructeur / destructeur
// ---------------------------------------------------------------------------

LoadingScreen::LoadingScreen() {
    m_shader      = compileProgram();
    m_timeLoc     = glGetUniformLocation(m_shader, "uTime");
    m_alphaLoc    = glGetUniformLocation(m_shader, "uAlpha");
    m_progressLoc = glGetUniformLocation(m_shader, "uProgress");
    m_stepLoc     = glGetUniformLocation(m_shader, "uStep");
    m_stepCntLoc  = glGetUniformLocation(m_shader, "uStepCount");
    glGenVertexArrays(1, &m_vao);
    logPrintf("[LoadingScreen] Initialise.\n");
}

LoadingScreen::~LoadingScreen() {
    if (m_shader) { glDeleteProgram(m_shader); m_shader = 0; }
    if (m_vao)    { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
}

// ---------------------------------------------------------------------------
// Rendu
// ---------------------------------------------------------------------------

void LoadingScreen::draw(float dt, float alpha) {
    m_time += dt;

    // Lerp fluide vers la progression cible
    if (m_displayProgress < m_targetProgress) {
        m_displayProgress += LERP_SPEED * dt;
        if (m_displayProgress > m_targetProgress)
            m_displayProgress = m_targetProgress;
    }

    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(m_shader);
    glUniform1f(m_timeLoc,     m_time);
    glUniform1f(m_alphaLoc,    alpha);
    glUniform1f(m_progressLoc, m_displayProgress);
    glUniform1i(m_stepLoc,     m_step);
    glUniform1i(m_stepCntLoc,  m_stepCount);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

// ---------------------------------------------------------------------------
// Étapes
// ---------------------------------------------------------------------------

void LoadingScreen::setStep(int current, int total) {
    m_step      = current;
    m_stepCount = total;
    m_targetProgress = (total > 0) ? (float)current / (float)total : 0.0f;
}

// ---------------------------------------------------------------------------
// Label texte (optionnel — nécessite setTextRenderer())
// ---------------------------------------------------------------------------

void LoadingScreen::drawLabel() {
    if (!m_textRenderer) return;
    const char* name = getStepName(m_step);
    if (!name) return;

    float scale = 0.48f;
    float w = m_textRenderer->getTextWidth(name, scale);
    float x = (Constants::Window::WINDOW_WIDTH - w) * 0.5f;
    // Couleur LINEN (#EFE6DD)
    m_textRenderer->renderText(name, x, 450.0f, scale,
                                0.937f, 0.902f, 0.867f);
}
