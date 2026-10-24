#pragma once

#include <cmath>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include <glm/glm.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// Mini-jeux des quetes : logique seule (aucun appel OpenGL, testable dans
// -selftest). Le dessin passe par QuestPainter, implemente par QuestManager.
// Coordonnees de dessin : pixels ecran, y vers le bas, zone de contenu `area`
// (x, y = coin haut-gauche ; z, w = largeur, hauteur).
// ─────────────────────────────────────────────────────────────────────────────

enum class QuestType { Wires, Simon, Timing, Wave };

// Entrees du mini-jeu (clavier ZQSD/fleches/Entree/Espace + croix/A de la manette) :
// fronts d'appui (pressed) et etat maintenu (held).
struct QuestInput {
    bool up = false, down = false, left = false, right = false, confirm = false;
    bool upHeld = false, downHeld = false, leftHeld = false, rightHeld = false;
};

class QuestPainter {
public:
    virtual ~QuestPainter() = default;
    // Rectangle plein centre en (cx, cy), rotation en degres.
    virtual void rect(float cx, float cy, float w, float h, const glm::vec3& color, float rotDeg = 0.0f) = 0;
    // Texte : align 0 = gauche, 1 = centre, 2 = droite ; y = milieu vertical du texte.
    virtual void text(const std::string& s, float x, float y, float scale, const glm::vec3& color, int align = 0) = 0;
};

class MiniGame {
public:
    virtual ~MiniGame() = default;
    virtual void update(float dt, const QuestInput& in) = 0;
    virtual void draw(QuestPainter& p, const glm::vec4& area, float time) const = 0;
    virtual const char* help() const = 0;     // commandes, affichees en bas du panneau
    virtual std::string status() const = 0;   // consigne / retour courant
    bool solved() const { return m_solved; }
    bool error() const { return m_error > 0.0f; }   // erreur recente (retour visuel)
protected:
    void tickError(float dt) { m_error = m_error > dt ? m_error - dt : 0.0f; }
    bool m_solved = false;
    float m_error = 0.0f;
};

std::unique_ptr<MiniGame> makeMiniGame(QuestType type, unsigned seed);
const char* questTypeName(QuestType type);      // "wires", "simon"... (noms des postes dans la carte)
bool parseQuestType(const std::string& s, QuestType& out);

// ── Rebrancher les cables : relier chaque couleur a gauche a sa prise a droite ──
class WiresGame : public MiniGame {
public:
    static constexpr int N = 4;
    explicit WiresGame(unsigned seed);
    void update(float dt, const QuestInput& in) override;
    void draw(QuestPainter& p, const glm::vec4& area, float time) const override;
    const char* help() const override { return "Haut/Bas : choisir   Entree : brancher   Gauche : revenir"; }
    std::string status() const override;
    // Tests : couleur de la prise de droite j, colonne et ligne du curseur.
    int socketColor(int j) const { return m_perm[j]; }
    int connectedCount() const;
    int side() const { return m_side; }
    int cursor() const { return m_cursor[m_side]; }
    int selectedCable() const { return m_cursor[0]; }
    bool socketUsed(int j) const { return m_used[j]; }
private:
    int m_perm[N];            // couleur de la prise de droite j
    bool m_linked[N] = {};    // cable gauche i branche
    bool m_used[N] = {};      // prise droite j occupee
    int m_side = 0;           // 0 : choix du cable (gauche), 1 : choix de la prise (droite)
    int m_cursor[2] = { 0, 0 };
};

// ── Pirater le digicode : memoriser puis rejouer une sequence (3 manches) ──
class SimonGame : public MiniGame {
public:
    static constexpr int ROUNDS = 3;
    explicit SimonGame(unsigned seed);
    void update(float dt, const QuestInput& in) override;
    void draw(QuestPainter& p, const glm::vec4& area, float time) const override;
    const char* help() const override { return "Fleches / ZQSD : rejouer la sequence"; }
    std::string status() const override;
    // Tests.
    bool waitingInput() const { return m_phase == Phase::Input; }
    int expected() const { return m_seq[m_step]; }   // pave attendu (0 haut, 1 droite, 2 bas, 3 gauche)
private:
    enum class Phase { Show, Input, Pause };
    int length() const { return 3 + m_round; }
    std::vector<int> m_seq;
    int m_round = 0, m_step = 0;
    Phase m_phase = Phase::Pause;
    float m_timer = 0.8f;
    int m_lit = -1;           // pave allume (-1 : aucun)
    float m_litTimer = 0.0f;
};

// ── Calibrer la chaudiere : arreter le curseur dans la zone, 3 fois ──
class TimingGame : public MiniGame {
public:
    static constexpr int HITS = 3;
    explicit TimingGame(unsigned seed);
    void update(float dt, const QuestInput& in) override;
    void draw(QuestPainter& p, const glm::vec4& area, float time) const override;
    const char* help() const override { return "Entree / Espace : bloquer le curseur"; }
    std::string status() const override;
    // Tests.
    bool cursorInZone() const { return std::abs(m_pos - m_zone) <= zoneHalf(); }
private:
    float zoneHalf() const { return 0.09f - 0.018f * m_hits; }
    void newZone();
    std::mt19937 m_rng;
    float m_pos = 0.0f, m_dir = 1.0f, m_zone = 0.5f;
    int m_hits = 0;
};

// ── Accorder la radio : caler frequence et amplitude sur l'onde cible ──
class WaveGame : public MiniGame {
public:
    explicit WaveGame(unsigned seed);
    void update(float dt, const QuestInput& in) override;
    void draw(QuestPainter& p, const glm::vec4& area, float time) const override;
    const char* help() const override { return "Gauche/Droite : frequence   Haut/Bas : amplitude"; }
    std::string status() const override;
    // Tests.
    float freqError() const { return m_freq - m_targetFreq; }
    float ampError() const { return m_amp - m_targetAmp; }
    bool locked() const;
private:
    float m_targetFreq, m_targetAmp;
    float m_freq, m_amp;
    float m_progress = 0.0f;
};
