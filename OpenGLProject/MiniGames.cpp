#include "MiniGames.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "constants/color.h"

namespace {
    using namespace Constants::Color;

    const glm::vec3 kColors[4] = { TOMATO_JAM, TROPICAL_TEAL, VANILLA_CUSTARD, LINEN };

    glm::vec3 dim(const glm::vec3& c, float k) { return c * k; }
    glm::vec3 lighten(const glm::vec3& c, float t) { return c + (glm::vec3(1.0f) - c) * t; }
    // Clignotement doux (0..1) pour les curseurs.
    float pulse(float time) { return 0.5f + 0.5f * std::sin(time * 6.0f); }

    // Segment epais (rectangle tourne) de a a b.
    void segment(QuestPainter& p, glm::vec2 a, glm::vec2 b, float thickness, const glm::vec3& c) {
        const glm::vec2 d = b - a;
        const float len = glm::length(d);
        if (len < 1e-3f) return;
        const glm::vec2 m = (a + b) * 0.5f;
        p.rect(m.x, m.y, len, thickness, c, glm::degrees(std::atan2(d.y, d.x)));
    }

    // Losange (carre tourne de 45 deg) avec liseré sombre.
    void diamond(QuestPainter& p, float cx, float cy, float size, const glm::vec3& c, const glm::vec3& border) {
        p.rect(cx, cy, size + 12.0f, size + 12.0f, border, 45.0f);
        p.rect(cx, cy, size, size, c, 45.0f);
    }

    unsigned mixSeed(unsigned seed, unsigned salt) { return seed * 2654435761u ^ (salt * 40503u + 0x9e3779b9u); }
}

const char* questTypeName(QuestType type) {
    switch (type) {
    case QuestType::Wires:  return "wires";
    case QuestType::Simon:  return "simon";
    case QuestType::Timing: return "timing";
    default:                return "wave";
    }
}

bool parseQuestType(const std::string& s, QuestType& out) {
    for (QuestType t : { QuestType::Wires, QuestType::Simon, QuestType::Timing, QuestType::Wave }) {
        if (s == questTypeName(t)) { out = t; return true; }
    }
    return false;
}

std::unique_ptr<MiniGame> makeMiniGame(QuestType type, unsigned seed) {
    switch (type) {
    case QuestType::Wires:  return std::make_unique<WiresGame>(seed);
    case QuestType::Simon:  return std::make_unique<SimonGame>(seed);
    case QuestType::Timing: return std::make_unique<TimingGame>(seed);
    default:                return std::make_unique<WaveGame>(seed);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Cables
// ─────────────────────────────────────────────────────────────────────────────

WiresGame::WiresGame(unsigned seed) {
    std::mt19937 rng(mixSeed(seed, 1));
    for (int j = 0; j < N; ++j) m_perm[j] = j;
    do { std::shuffle(m_perm, m_perm + N, rng); } while (m_perm[0] == 0 && m_perm[1] == 1);   // pas (presque) l'identite
}

int WiresGame::connectedCount() const {
    return static_cast<int>(std::count(m_linked, m_linked + N, true));
}

void WiresGame::update(float dt, const QuestInput& in) {
    tickError(dt);
    if (m_solved) return;
    int& c = m_cursor[m_side];
    if (in.up) c = (c + N - 1) % N;
    if (in.down) c = (c + 1) % N;
    if (m_side == 0) {
        if (in.confirm && !m_linked[c]) {
            m_side = 1;
            m_cursor[1] = static_cast<int>(std::find(m_used, m_used + N, false) - m_used);
        }
        return;
    }
    if (in.left) { m_side = 0; return; }
    if (!in.confirm || m_used[c]) return;
    const int cable = m_cursor[0];
    if (m_perm[c] == cable) {
        m_linked[cable] = m_used[c] = true;
        m_solved = connectedCount() == N;
        for (int k = 1; k <= N; ++k) {   // curseur sur le prochain cable libre
            if (!m_linked[(cable + k) % N]) { m_cursor[0] = (cable + k) % N; break; }
        }
    } else {
        m_error = 0.6f;
    }
    m_side = 0;
}

std::string WiresGame::status() const {
    if (m_solved) return "Courant retabli";
    if (error()) return "Mauvaise prise !";
    return m_side == 0 ? "Choisissez un cable" : "Branchez-le sur la prise de meme couleur";
}

void WiresGame::draw(QuestPainter& p, const glm::vec4& a, float time) const {
    const float lx = a.x + a.z * 0.16f, rx = a.x + a.z * 0.84f;
    auto rowY = [&](int i) { return a.y + a.w * (0.14f + 0.24f * i); };
    const glm::vec3 plate = lighten(SHADOW_GREY, 0.06f);

    // Platines
    p.rect(lx, a.y + a.w * 0.5f, 190.0f, a.w * 1.02f, dim(SHADOW_GREY, 0.7f));
    p.rect(lx, a.y + a.w * 0.5f, 176.0f, a.w * 0.99f, plate);
    p.rect(rx, a.y + a.w * 0.5f, 190.0f, a.w * 1.02f, dim(SHADOW_GREY, 0.7f));
    p.rect(rx, a.y + a.w * 0.5f, 176.0f, a.w * 0.99f, plate);

    // Cables branches (ombre puis cable) et apercu du cable en cours
    for (int j = 0; j < N; ++j) {
        if (!m_used[j]) continue;
        const int i = m_perm[j];
        const glm::vec2 A(lx + 62.0f, rowY(i)), B(rx - 42.0f, rowY(j));
        segment(p, A + glm::vec2(0, 5), B + glm::vec2(0, 5), 20.0f, dim(SHADOW_GREY, 0.5f));
        segment(p, A, B, 13.0f, kColors[i]);
        segment(p, A + glm::vec2(0, -3), B + glm::vec2(0, -3), 3.0f, lighten(kColors[i], 0.35f));
    }
    if (m_side == 1) {
        const int i = m_cursor[0];
        segment(p, glm::vec2(lx + 62.0f, rowY(i)), glm::vec2(rx - 42.0f, rowY(m_cursor[1])), 7.0f,
                dim(kColors[i], 0.55f + 0.3f * pulse(time)));
    }

    // Fiches (gauche) et prises (droite)
    for (int i = 0; i < N; ++i) {
        const float y = rowY(i);
        if (m_side == 0 && m_cursor[0] == i) p.rect(lx, y, 150.0f, 78.0f, lighten(VANILLA_CUSTARD, 0.0f) * (0.6f + 0.4f * pulse(time)));
        else if (m_side == 1 && m_cursor[0] == i) p.rect(lx, y, 150.0f, 78.0f, VANILLA_CUSTARD);
        p.rect(lx, y, 128.0f, 58.0f, dim(SHADOW_GREY, 0.6f));
        p.rect(lx, y, 112.0f, 42.0f, m_linked[i] ? dim(kColors[i], 0.75f) : kColors[i]);
        p.rect(lx + 62.0f, y, 26.0f, 22.0f, LINEN);
        p.rect(lx - 18.0f, y - 8.0f, 54.0f, 6.0f, lighten(kColors[i], 0.4f));   // reflet
    }
    for (int j = 0; j < N; ++j) {
        const float y = rowY(j);
        const glm::vec3 c = kColors[m_perm[j]];
        if (m_side == 1 && m_cursor[1] == j) p.rect(rx, y, 120.0f, 94.0f, (m_error > 0.0f ? TOMATO_JAM : VANILLA_CUSTARD) * (0.6f + 0.4f * pulse(time)));
        p.rect(rx, y, 96.0f, 72.0f, dim(SHADOW_GREY, 0.55f));
        p.rect(rx, y, 76.0f, 52.0f, m_used[j] ? c : dim(c, 0.45f));
        p.rect(rx - 46.0f, y, 14.0f, 30.0f, m_used[j] ? LINEN : dim(LINEN, 0.35f));
        p.rect(rx + 24.0f, y, 10.0f, 10.0f, m_used[j] ? TROPICAL_TEAL : dim(TOMATO_JAM, 0.8f), 45.0f);   // voyant
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Digicode (Simon)
// ─────────────────────────────────────────────────────────────────────────────

SimonGame::SimonGame(unsigned seed) {
    std::mt19937 rng(mixSeed(seed, 2));
    std::uniform_int_distribution<int> pad(0, 3);
    for (int i = 0; i < 3 + ROUNDS - 1; ++i) {
        int v = pad(rng);
        if (!m_seq.empty() && v == m_seq.back()) v = (v + 1 + pad(rng) % 3) % 4;   // pas deux fois le meme pave
        m_seq.push_back(v);
    }
}

namespace { constexpr float kStepTime = 0.75f, kLitTime = 0.55f; }

void SimonGame::update(float dt, const QuestInput& in) {
    tickError(dt);
    if (m_solved) return;
    if (m_phase == Phase::Pause) {
        m_lit = m_litTimer > 0.0f ? m_lit : -1;
        m_litTimer = std::max(0.0f, m_litTimer - dt);
        m_timer -= dt;
        if (m_timer <= 0.0f) { m_phase = Phase::Show; m_timer = 0.0f; }
        return;
    }
    if (m_phase == Phase::Show) {
        m_timer += dt;
        const int idx = static_cast<int>(m_timer / kStepTime);
        if (idx >= length()) { m_phase = Phase::Input; m_step = 0; m_lit = -1; return; }
        m_lit = std::fmod(m_timer, kStepTime) < kLitTime ? m_seq[idx] : -1;
        return;
    }
    // Saisie
    m_litTimer = std::max(0.0f, m_litTimer - dt);
    if (m_litTimer <= 0.0f) m_lit = -1;
    const int pressed = in.up ? 0 : in.right ? 1 : in.down ? 2 : in.left ? 3 : -1;
    if (pressed < 0) return;
    m_lit = pressed;
    m_litTimer = 0.22f;
    if (pressed != m_seq[m_step]) {          // erreur : on rejoue la meme manche
        m_error = 0.9f;
        m_phase = Phase::Pause;
        m_timer = 1.1f;
        return;
    }
    if (++m_step < length()) return;
    if (++m_round >= ROUNDS) { m_solved = true; return; }
    m_phase = Phase::Pause;
    m_timer = 0.7f;
}

std::string SimonGame::status() const {
    if (m_solved) return "Acces autorise";
    if (error()) return "Code refuse ! Nouvelle tentative";
    if (m_phase == Phase::Input) return "A vous : " + std::to_string(m_step) + " / " + std::to_string(length());
    return "Observez la sequence...";
}

void SimonGame::draw(QuestPainter& p, const glm::vec4& a, float time) const {
    // Le socle tourne de 45 deg occupe 3.1 * d * sqrt(2) en hauteur : d borne pour rester dans la zone.
    const float cx = a.x + a.z * 0.5f, cy = a.y + a.w * 0.44f;
    const float d = std::min(a.z, a.w) * 0.185f, s = d * 0.92f;
    const glm::vec2 offs[4] = { { 0, -1 }, { 1, 0 }, { 0, 1 }, { -1, 0 } };
    p.rect(cx, cy, d * 3.3f, d * 3.3f, dim(SHADOW_GREY, 0.75f), 45.0f);   // socle
    p.rect(cx, cy, d * 3.1f, d * 3.1f, lighten(SHADOW_GREY, 0.05f), 45.0f);
    for (int i = 0; i < 4; ++i) {
        const bool lit = m_lit == i;
        const glm::vec3 c = lit ? lighten(kColors[i], 0.3f) : dim(kColors[i], 0.38f);
        const float size = lit ? s * 1.08f : s;
        diamond(p, cx + offs[i].x * d, cy + offs[i].y * d, size, c, lit ? lighten(kColors[i], 0.6f) : dim(SHADOW_GREY, 0.55f));
        p.rect(cx + offs[i].x * d - size * 0.12f, cy + offs[i].y * d - size * 0.12f, size * 0.35f, size * 0.08f,
               lit ? glm::vec3(1.0f) : dim(kColors[i], 0.55f), 45.0f);   // facette claire
    }
    // Manche au centre
    diamond(p, cx, cy, d * 0.55f, m_error > 0.0f ? TOMATO_JAM : SHADOW_GREY, dim(VANILLA_CUSTARD, 0.6f));
    p.text(std::to_string(std::min(m_round + 1, ROUNDS)) + "/" + std::to_string(ROUNDS), cx, cy, 0.32f, VANILLA_CUSTARD, 1);
    // Progression de la saisie
    const int n = length();
    for (int k = 0; k < n; ++k) {
        const float x = cx + (k - (n - 1) * 0.5f) * 34.0f, y = a.y + a.w * 0.98f;
        const bool done = m_phase == Phase::Input && k < m_step;
        diamond(p, x, y, 14.0f, done ? TROPICAL_TEAL : dim(LINEN, 0.25f), dim(SHADOW_GREY, 0.5f));
    }
    (void)time;
}

// ─────────────────────────────────────────────────────────────────────────────
// Chaudiere (timing)
// ─────────────────────────────────────────────────────────────────────────────

TimingGame::TimingGame(unsigned seed) : m_rng(mixSeed(seed, 3)) { newZone(); }

void TimingGame::newZone() {
    std::uniform_real_distribution<float> z(0.15f, 0.85f);
    do { m_zone = z(m_rng); } while (std::abs(m_zone - m_pos) < 0.25f);
}

void TimingGame::update(float dt, const QuestInput& in) {
    tickError(dt);
    if (m_solved) return;
    m_pos += m_dir * (0.55f + 0.22f * m_hits) * dt;
    if (m_pos > 1.0f) { m_pos = 2.0f - m_pos; m_dir = -1.0f; }
    if (m_pos < 0.0f) { m_pos = -m_pos; m_dir = 1.0f; }
    if (!in.confirm) return;
    if (cursorInZone()) {
        if (++m_hits >= HITS) m_solved = true;
        else newZone();
    } else {
        m_error = 0.5f;
        m_hits = std::max(0, m_hits - 1);
    }
}

std::string TimingGame::status() const {
    if (m_solved) return "Pression stabilisee";
    if (error()) return "Rate ! La pression remonte";
    return "Bloquez le curseur dans la zone (" + std::to_string(m_hits) + " / " + std::to_string(HITS) + ")";
}

void TimingGame::draw(QuestPainter& p, const glm::vec4& a, float time) const {
    const float x0 = a.x + a.z * 0.07f, x1 = a.x + a.z * 0.93f, w = x1 - x0;
    const float cy = a.y + a.w * 0.40f, h = 74.0f;
    p.rect(x0 + w * 0.5f, cy, w + 22.0f, h + 22.0f, dim(SHADOW_GREY, 0.6f));
    p.rect(x0 + w * 0.5f, cy, w + 10.0f, h + 10.0f, dim(TOMATO_JAM, 0.55f));
    p.rect(x0 + w * 0.5f, cy, w, h, lighten(SHADOW_GREY, 0.04f));
    // Graduations (degrade froid -> chaud)
    for (int k = 0; k <= 20; ++k) {
        const float t = k / 20.0f;
        p.rect(x0 + w * t, cy + h * 0.5f + 16.0f, 4.0f, k % 5 ? 12.0f : 24.0f, dim(glm::mix(TROPICAL_TEAL, TOMATO_JAM, t), 0.9f));
    }
    // Zone cible
    const float half = zoneHalf();
    p.rect(x0 + w * m_zone, cy, w * half * 2.0f, h - 8.0f, dim(TROPICAL_TEAL, 0.75f + 0.25f * pulse(time)));
    p.rect(x0 + w * (m_zone - half), cy, 5.0f, h + 6.0f, lighten(TROPICAL_TEAL, 0.4f));
    p.rect(x0 + w * (m_zone + half), cy, 5.0f, h + 6.0f, lighten(TROPICAL_TEAL, 0.4f));
    // Curseur
    const float cx = x0 + w * m_pos;
    p.rect(cx, cy, 22.0f, h + 44.0f, dim(SHADOW_GREY, 0.4f));
    p.rect(cx, cy, 12.0f, h + 34.0f, m_error > 0.0f ? TOMATO_JAM : VANILLA_CUSTARD);
    p.rect(cx, cy - h * 0.5f - 26.0f, 26.0f, 26.0f, m_error > 0.0f ? TOMATO_JAM : VANILLA_CUSTARD, 45.0f);
    // Reussites
    for (int k = 0; k < HITS; ++k) {
        const float x = a.x + a.z * 0.5f + (k - 1) * 90.0f, y = a.y + a.w * 0.84f;
        diamond(p, x, y, 40.0f, k < m_hits ? TROPICAL_TEAL : dim(LINEN, 0.18f), k < m_hits ? lighten(TROPICAL_TEAL, 0.5f) : dim(SHADOW_GREY, 0.5f));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Radio (onde)
// ─────────────────────────────────────────────────────────────────────────────

namespace {
    constexpr float kFreqMin = 1.0f, kFreqMax = 4.5f, kAmpMin = 0.2f, kAmpMax = 1.0f;
    constexpr float kFreqTol = 0.10f, kAmpTol = 0.05f;
}

WaveGame::WaveGame(unsigned seed) {
    std::mt19937 rng(mixSeed(seed, 4));
    m_targetFreq = std::uniform_real_distribution<float>(1.5f, 4.0f)(rng);
    m_targetAmp = std::uniform_real_distribution<float>(0.35f, 0.95f)(rng);
    m_freq = std::clamp(m_targetFreq + (m_targetFreq > 2.75f ? -1.3f : 1.3f), kFreqMin, kFreqMax);
    m_amp = std::clamp(m_targetAmp + (m_targetAmp > 0.6f ? -0.32f : 0.32f), kAmpMin, kAmpMax);
}

bool WaveGame::locked() const {
    return std::abs(m_freq - m_targetFreq) < kFreqTol && std::abs(m_amp - m_targetAmp) < kAmpTol;
}

void WaveGame::update(float dt, const QuestInput& in) {
    tickError(dt);
    if (m_solved) return;
    if (in.leftHeld) m_freq -= 1.1f * dt;
    if (in.rightHeld) m_freq += 1.1f * dt;
    if (in.upHeld) m_amp += 0.5f * dt;
    if (in.downHeld) m_amp -= 0.5f * dt;
    m_freq = std::clamp(m_freq, kFreqMin, kFreqMax);
    m_amp = std::clamp(m_amp, kAmpMin, kAmpMax);
    m_progress = std::clamp(m_progress + (locked() ? dt / 1.3f : -dt * 0.6f), 0.0f, 1.0f);
    m_solved = m_progress >= 1.0f;
}

std::string WaveGame::status() const {
    if (m_solved) return "Radio accordee";
    return locked() ? "Signal capte... ne bougez plus" : "Cherchez le signal";
}

void WaveGame::draw(QuestPainter& p, const glm::vec4& a, float time) const {
    const float sx0 = a.x + a.z * 0.04f, sx1 = a.x + a.z * 0.96f, sw = sx1 - sx0;
    const float sy0 = a.y, sh = a.w * 0.70f, cy = sy0 + sh * 0.5f;
    p.rect(sx0 + sw * 0.5f, cy, sw + 16.0f, sh + 16.0f, dim(SHADOW_GREY, 0.6f));
    p.rect(sx0 + sw * 0.5f, cy, sw, sh, glm::mix(dim(SHADOW_GREY, 0.8f), TROPICAL_TEAL, 0.06f));
    for (int k = 1; k < 10; ++k) p.rect(sx0 + sw * k / 10.0f, cy, 2.0f, sh, glm::mix(SHADOW_GREY, TROPICAL_TEAL, 0.16f));
    for (int k = 1; k < 6; ++k) p.rect(sx0 + sw * 0.5f, sy0 + sh * k / 6.0f, sw, 2.0f, glm::mix(SHADOW_GREY, TROPICAL_TEAL, k == 3 ? 0.3f : 0.16f));

    auto wave = [&](float freq, float amp, float size, const glm::vec3& c) {
        const int n = 120;
        for (int i = 0; i <= n; ++i) {
            const float u = i / static_cast<float>(n);
            const float y = cy - amp * sh * 0.45f * std::sin(6.2831853f * freq * u - time * 2.2f);
            p.rect(sx0 + sw * u, y, size, size, c, 45.0f);
        }
    };
    wave(m_targetFreq, m_targetAmp, 8.0f, dim(TROPICAL_TEAL, 0.6f));
    wave(m_freq, m_amp, 10.0f, locked() ? lighten(TROPICAL_TEAL, 0.35f) : VANILLA_CUSTARD);

    // Reception
    const float by = a.y + a.w * 0.84f;
    p.rect(sx0 + sw * 0.5f, by, sw * 0.6f + 10.0f, 30.0f, dim(SHADOW_GREY, 0.6f));
    p.rect(sx0 + sw * 0.2f + sw * 0.3f * m_progress, by, sw * 0.6f * m_progress, 20.0f, TROPICAL_TEAL);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "FREQ %.2f", m_freq);
    p.text(buf, sx0 + 10.0f, by, 0.32f, m_error > 0.0f ? TOMATO_JAM : LINEN, 0);
    std::snprintf(buf, sizeof(buf), "AMPL %d%%", static_cast<int>(std::round(m_amp * 100.0f)));
    p.text(buf, sx1 - 10.0f, by, 0.32f, LINEN, 2);
}
