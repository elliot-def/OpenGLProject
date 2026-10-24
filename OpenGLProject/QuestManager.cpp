#include "QuestManager.h"

#include <algorithm>
#include <random>

#include "Image.h"
#include "Log.h"
#include "MapLoader.h"
#include "Rectangle.h"
#include "Scene.h"
#include "ShaderManager.h"
#include "TextRenderer.h"

#include "constants/color.h"
#include "constants/window.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace {
    using namespace Constants::Color;
    constexpr const char* kPrefix = "quest:";
    constexpr const char* kDir = "./res/textures/quests/";
    constexpr float kPanelW = 1180.0f, kPanelH = 760.0f;   // taille native de panel.png
    constexpr float kSuccessTime = 1.4f;
    constexpr float kHudX = 30.0f, kHudY = 112.0f, kHudW = 480.0f, kHudLine = 46.0f;
}

// ── Pinceau de l'UI : rectangles (shader "shape"), images, texte (police 0 / titres 1) ──
class QuestManager::Painter : public QuestPainter {
public:
    Painter(class Rectangle* rect, std::vector<std::unique_ptr<TextRenderer>>* trs) : m_rect(rect), m_trs(trs) {}

    void rect(float cx, float cy, float w, float h, const glm::vec3& c, float rotDeg = 0.0f) override {
        if (!m_rect) return;
        m_rect->setPosition(cx, cy);
        m_rect->setSize(w, h);
        m_rect->setColor(c.r, c.g, c.b);
        m_rect->setRotation(rotDeg);
        m_rect->draw();
    }
    void text(const std::string& s, float x, float y, float scale, const glm::vec3& c, int align = 0) override {
        drawText(0, s, x, y, scale, c, align);
    }
    void title(const std::string& s, float x, float y, float scale, const glm::vec3& c, int align = 0) {
        drawText(m_trs && m_trs->size() > 1 ? 1 : 0, s, x, y, scale, c, align);
    }
    void image(Image* img, float cx, float cy, float w, float h, float opacity = 1.0f) {
        if (!img) return;
        img->setPosition(cx, cy);
        img->setSize(w, h);
        img->setOpacity(opacity);
        img->draw();
    }
private:
    void drawText(size_t font, const std::string& s, float x, float y, float scale, const glm::vec3& c, int align) {
        if (!m_trs || m_trs->size() <= font || s.empty()) return;
        TextRenderer* tr = (*m_trs)[font].get();
        const float w = tr->getTextWidth(s, scale);
        const float baseline = y + tr->getTextHeight("H", scale) * 0.5f;   // y = milieu du texte
        const float left = align == 1 ? x - w * 0.5f : (align == 2 ? x - w : x);
        tr->renderText(s, left + 2.0f, baseline + 2.0f, scale, 0.0f, 0.0f, 0.0f);   // ombre portee
        tr->renderText(s, left, baseline, scale, c.r, c.g, c.b);
    }
    class Rectangle* m_rect;
    std::vector<std::unique_ptr<TextRenderer>>* m_trs;
};

QuestManager::QuestManager(GLFWwindow* window, ShaderManager* shaderManager,
                           std::vector<std::unique_ptr<TextRenderer>>* textRenderers)
    : m_window(window), m_shaderManager(shaderManager), m_textRenderers(textRenderers) {
    if (!m_shaderManager) return;
    m_rect = std::make_unique<class Rectangle>(m_shaderManager->getShader("shape"), 0.0f, 0.0f, 1.0f, 1.0f, SHADOW_GREY);
    Shader* img = m_shaderManager->getShader("image");
    auto load = [&](const char* file) { return std::make_unique<Image>(img, std::string(kDir) + file, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f); };
    m_dim = load("dim.png");
    m_panel = load("panel.png");
    m_hudPanel = load("hud_panel.png");
    m_done = load("task_done.png");
    m_todo = load("task_todo.png");
    m_icons[static_cast<int>(QuestType::Wires)] = load("icon_wires.png");
    m_icons[static_cast<int>(QuestType::Simon)] = load("icon_simon.png");
    m_icons[static_cast<int>(QuestType::Timing)] = load("icon_timing.png");
    m_icons[static_cast<int>(QuestType::Wave)] = load("icon_wave.png");
}

QuestManager::~QuestManager() = default;

int QuestManager::completed() const {
    return static_cast<int>(std::count_if(m_tasks.begin(), m_tasks.end(), [](const Task& t) { return t.done; }));
}

// ── Carte ───────────────────────────────────────────────────────────────────

void QuestManager::beginMap(Scene* scene, int count, unsigned seed) {
    m_game.reset();
    m_current = -1;
    m_tasks.clear();
    m_scene = scene;
    m_seed = seed;
    if (!scene) return;

    std::vector<Task> all;
    for (const MapButton& b : scene->getMapButtons()) {
        if (b.name.rfind(kPrefix, 0) != 0) continue;
        const std::string rest = b.name.substr(std::char_traits<char>::length(kPrefix));
        Task t;
        if (!parseQuestType(rest.substr(0, rest.find(':')), t.type)) {
            LOG_WARN("[Quest] Type de poste inconnu : %s", b.name.c_str());
            continue;
        }
        t.button = b.name;
        t.label = b.label;
        all.push_back(t);
    }
    // Tirage commun (graine du lobby) : memes postes chez tous les joueurs.
    std::mt19937 rng(seed * 2654435761u + 17u);
    std::shuffle(all.begin(), all.end(), rng);
    const size_t n = std::min(all.size(), static_cast<size_t>(std::max(0, count)));
    for (size_t i = 0; i < all.size(); ++i) {
        scene->setButtonEnabled(all[i].button, i < n);
        if (i < n) m_tasks.push_back(all[i]);
    }
    if (!m_tasks.empty()) LOG_INFO("[Quest] %zu taches tirees sur %zu postes", m_tasks.size(), all.size());
}

bool QuestManager::handleButton(const std::string& name) {
    if (name.rfind(kPrefix, 0) != 0) return false;
    for (size_t i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks[i].button != name || m_tasks[i].done) continue;
        m_current = static_cast<int>(i);
        m_game = makeMiniGame(m_tasks[i].type, m_seed * 31u + static_cast<unsigned>(i) * 7919u + static_cast<unsigned>(m_time * 1000.0f));
        m_successTimer = 0.0f;
        m_openFade = 0.0f;
        readInput();   // synchronise les fronts : une touche deja enfoncee ne compte pas
        m_escPrev = false;
        return true;
    }
    return true;   // poste inactif ou deja fait : rien
}

void QuestManager::close() {
    m_game.reset();
    m_current = -1;
}

// ── Entrees ─────────────────────────────────────────────────────────────────

QuestInput QuestManager::readInput() {
    QuestInput in;
    if (!m_window) return in;
    auto key = [&](int k) { return glfwGetKey(m_window, k) == GLFW_PRESS; };
    bool held[5] = {
        key(GLFW_KEY_UP) || key(GLFW_KEY_W) || key(GLFW_KEY_Z),
        key(GLFW_KEY_DOWN) || key(GLFW_KEY_S),
        key(GLFW_KEY_LEFT) || key(GLFW_KEY_A) || key(GLFW_KEY_Q),
        key(GLFW_KEY_RIGHT) || key(GLFW_KEY_D),
        key(GLFW_KEY_ENTER) || key(GLFW_KEY_KP_ENTER) || key(GLFW_KEY_SPACE),
    };
    bool esc = key(GLFW_KEY_ESCAPE);
    GLFWgamepadstate pad;
    if (glfwJoystickIsGamepad(GLFW_JOYSTICK_1) && glfwGetGamepadState(GLFW_JOYSTICK_1, &pad)) {
        const float ax = pad.axes[GLFW_GAMEPAD_AXIS_LEFT_X], ay = pad.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
        held[0] |= pad.buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] || ay < -0.6f;
        held[1] |= pad.buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] || ay > 0.6f;
        held[2] |= pad.buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT] || ax < -0.6f;
        held[3] |= pad.buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] || ax > 0.6f;
        held[4] |= pad.buttons[GLFW_GAMEPAD_BUTTON_A] != 0;
        esc |= pad.buttons[GLFW_GAMEPAD_BUTTON_B] != 0;
    }
    bool* pressed[5] = { &in.up, &in.down, &in.left, &in.right, &in.confirm };
    for (int i = 0; i < 5; ++i) {
        *pressed[i] = held[i] && !m_prev[i];
        m_prev[i] = held[i];
    }
    in.upHeld = held[0]; in.downHeld = held[1]; in.leftHeld = held[2]; in.rightHeld = held[3];
    // Echap / B : au RELACHEMENT (sinon la touche encore enfoncee ouvrirait le menu pause).
    m_prev[5] = !esc && m_escPrev;
    m_escPrev = esc;
    return in;
}

// ── Mise a jour ─────────────────────────────────────────────────────────────

void QuestManager::update(float dt) {
    m_time += dt;
    m_hudFlash = std::max(0.0f, m_hudFlash - dt);
    if (!m_game) return;
    m_openFade = std::min(1.0f, m_openFade + dt * 6.0f);
    const QuestInput in = readInput();

    if (m_successTimer > 0.0f) {          // tache reussie : on laisse le message, puis on coche
        m_successTimer -= dt;
        if (m_successTimer > 0.0f) return;
        Task& t = m_tasks[m_current];
        t.done = true;
        if (m_scene) m_scene->setButtonEnabled(t.button, false);
        close();
        m_hudFlash = 1.6f;
        LOG_INFO("[Quest] Tache accomplie : %s (%d / %d)", t.label.c_str(), completed(), total());
        if (completed() == total() && m_onAllDone) m_onAllDone();
        return;
    }
    if (m_prev[5]) { close(); return; }   // Echap relache : abandon (le mini-jeu repart de zero)
    m_game->update(dt, in);
    if (m_game->solved()) m_successTimer = kSuccessTime;
}

// ── Rendu ───────────────────────────────────────────────────────────────────

void QuestManager::draw() {
    if (m_tasks.empty()) return;
    Painter p(m_rect.get(), m_textRenderers);
    glDisable(GL_DEPTH_TEST);
    drawHud(p);
    if (m_game) drawGame(p);
    glEnable(GL_DEPTH_TEST);
}

void QuestManager::drawHud(Painter& p) {
    const float h = 78.0f + kHudLine * m_tasks.size();
    p.image(m_hudPanel.get(), kHudX + kHudW * 0.5f, kHudY + h * 0.5f, kHudW, h, 0.92f);
    p.text("TACHES", kHudX + 34.0f, kHudY + 36.0f, 0.40f, VANILLA_CUSTARD);
    p.text(std::to_string(completed()) + " / " + std::to_string(total()), kHudX + kHudW - 30.0f, kHudY + 36.0f, 0.40f,
           completed() == total() ? TROPICAL_TEAL : LINEN, 2);
    // Barre de progression sous le titre
    const float bw = kHudW - 64.0f, frac = total() ? completed() / static_cast<float>(total()) : 0.0f;
    p.rect(kHudX + 32.0f + bw * 0.5f, kHudY + 62.0f, bw, 6.0f, SHADOW_GREY * 0.6f);
    if (frac > 0.0f) p.rect(kHudX + 32.0f + bw * frac * 0.5f, kHudY + 62.0f, bw * frac, 6.0f, TROPICAL_TEAL);

    for (size_t i = 0; i < m_tasks.size(); ++i) {
        const Task& t = m_tasks[i];
        const float y = kHudY + 78.0f + kHudLine * (i + 0.5f);
        const bool active = m_current == static_cast<int>(i);
        p.image(t.done ? m_done.get() : m_todo.get(), kHudX + 50.0f, y, 32.0f, 32.0f);
        glm::vec3 c = t.done ? TROPICAL_TEAL * 0.8f : (active ? VANILLA_CUSTARD : LINEN);
        if (t.done && m_hudFlash > 0.0f && completed() > 0) c = glm::mix(TROPICAL_TEAL * 0.8f, LINEN, std::min(1.0f, m_hudFlash));
        p.text(t.label, kHudX + 82.0f, y, 0.32f, c);
        if (t.done) p.rect(kHudX + 82.0f + 4.0f + (kHudW - 140.0f) * 0.5f, y + 1.0f, kHudW - 140.0f, 2.0f, TROPICAL_TEAL * 0.6f);   // barre
    }
}

void QuestManager::drawGame(Painter& p) {
    const float W = static_cast<float>(Constants::Window::WINDOW_WIDTH);
    const float H = static_cast<float>(Constants::Window::WINDOW_HEIGHT);
    const float cx = W * 0.5f, cy = H * 0.5f;
    const float left = cx - kPanelW * 0.5f, top = cy - kPanelH * 0.5f, right = cx + kPanelW * 0.5f, bottom = cy + kPanelH * 0.5f;
    const Task& t = m_tasks[m_current];
    const bool success = m_successTimer > 0.0f;

    p.image(m_dim.get(), cx, cy, W, H, 0.62f * m_openFade);
    const float s = 0.94f + 0.06f * m_openFade;   // leger zoom a l'ouverture
    p.image(m_panel.get(), cx, cy, kPanelW * s, kPanelH * s, m_openFade);
    if (m_openFade < 1.0f) return;

    // Bandeau de titre
    p.image(m_icons[static_cast<int>(t.type)].get(), left + 96.0f, top + 66.0f, 104.0f, 104.0f);
    p.title(t.label, left + 168.0f, top + 62.0f, 0.19f, TOMATO_JAM);
    p.text("Tache " + std::to_string(m_current + 1) + " / " + std::to_string(total()), right - 52.0f, top + 62.0f, 0.30f, TROPICAL_TEAL, 2);

    if (success) {
        // Bandeau de reussite
        p.rect(cx, cy, kPanelW - 120.0f, 150.0f, SHADOW_GREY * 0.7f);
        p.rect(cx, cy - 75.0f, kPanelW - 120.0f, 4.0f, TROPICAL_TEAL);
        p.rect(cx, cy + 75.0f, kPanelW - 120.0f, 4.0f, TROPICAL_TEAL);
        p.title("TACHE ACCOMPLIE", cx, cy - 18.0f, 0.24f, VANILLA_CUSTARD, 1);
        p.text(m_game->status(), cx, cy + 40.0f, 0.36f, TROPICAL_TEAL, 1);
        return;
    }

    // Consigne, contenu, aide
    p.text(m_game->status(), cx, top + 168.0f, 0.40f, m_game->error() ? TOMATO_JAM : VANILLA_CUSTARD, 1);
    const glm::vec4 area(left + 80.0f, top + 222.0f, kPanelW - 160.0f, kPanelH - 222.0f - 118.0f);
    m_game->draw(p, area, m_time);
    p.rect(cx, bottom - 92.0f, kPanelW - 120.0f, 2.0f, TOMATO_JAM * 0.6f);
    p.text(m_game->help(), left + 60.0f, bottom - 58.0f, 0.30f, TROPICAL_TEAL);
    p.text("[Echap] Abandonner", right - 60.0f, bottom - 58.0f, 0.30f, LINEN * 0.75f, 2);
}
