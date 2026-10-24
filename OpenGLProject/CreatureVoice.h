#pragma once

#include <random>
#include <vector>
#include <glm/glm.hpp>

class SoundManager;
class Sound;
class Creature;
class CollisionManager;

// ─────────────────────────────────────────────────────────────────────────────
// CreatureVoice : fropy "parle". Voice lines (res/sounds/fropy/voicelines,
// generees par make_voicelines.py : phrases isolees + bitcrush, mono 16 bits)
// jouees au hasard, comme une voix du chat vocal mais SPATIALISEE a la tete de
// fropy : source non relative (attenuation par la distance du SoundManager),
// etouffee (passe-bas) derriere un mur.
// ─────────────────────────────────────────────────────────────────────────────
class CreatureVoice {
public:
    explicit CreatureVoice(SoundManager* soundManager);

    // Chaque frame. creature null (lobby, mode Exploration) : silence.
    void update(float dt, const Creature* creature, const glm::vec3& listenerPos,
                const CollisionManager* collisionManager);

private:
    void stopCurrent();

    std::vector<Sound*> m_lines;
    Sound* m_current = nullptr;
    int    m_last = -1;
    float  m_wait = 0.0f;          // s avant la prochaine ligne
    float  m_occlusionTimer = 0.0f;
    std::mt19937 m_rng{ std::random_device{}() };
};
