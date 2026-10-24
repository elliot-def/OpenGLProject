#include "CreatureVoice.h"

#include <algorithm>
#include <filesystem>
#include <string>

#include "CollisionManager.h"
#include "Creature.h"
#include "Log.h"
#include "Sound.h"
#include "SoundManager.h"
#include "constants/creature.h"
#include "constants/resource.h"

namespace fs = std::filesystem;

CreatureVoice::CreatureVoice(SoundManager* soundManager) {
    std::error_code ec;
    std::vector<std::string> files;
    for (const auto& e : fs::directory_iterator(Constants::Resource::SOUND_FROPY_VOICELINES_DIR, ec)) {
        if (e.path().extension() == ".wav") files.push_back(e.path().generic_string());
    }
    std::sort(files.begin(), files.end());
    for (const std::string& f : files) {
        Sound* s = soundManager->load("fropy_voice_" + fs::path(f).stem().string(), f, false, 1.0f, 1.0f);
        if (!s) continue;
        s->setDistanceModel(Constants::Creature::VOICE_REFERENCE_DISTANCE,
                            Constants::Creature::VOICE_MAX_DISTANCE, 1.0f);
        m_lines.push_back(s);
    }
    m_wait = Constants::Creature::VOICE_GAP_MIN;
    LOG_INFO("[CreatureVoice] %zu voice lines chargees", m_lines.size());
}

void CreatureVoice::stopCurrent() {
    if (m_current) m_current->stop();
    m_current = nullptr;
}

void CreatureVoice::update(float dt, const Creature* creature, const glm::vec3& listenerPos,
                           const CollisionManager* collisionManager) {
    if (!creature || m_lines.empty()) { stopCurrent(); return; }

    const glm::vec3 mouth = creature->getPosition() + glm::vec3(0.0f, Constants::Creature::HEAD_BASE_Y, 0.0f);
    if (m_current && m_current->isPlaying()) {
        m_current->setPosition(mouth);   // fropy se deplace pendant qu'il parle
    } else {
        m_current = nullptr;
        m_wait -= dt;
        if (m_wait > 0.0f) return;
        std::uniform_int_distribution<int> pick(0, static_cast<int>(m_lines.size()) - 1);
        int i = pick(m_rng);
        if (i == m_last && m_lines.size() > 1) i = (i + 1) % static_cast<int>(m_lines.size());   // pas deux fois la meme
        m_last = i;
        m_current = m_lines[i];
        m_current->setPosition(mouth);
        m_current->play();
        m_wait = std::uniform_real_distribution<float>(Constants::Creature::VOICE_GAP_MIN,
                                                       Constants::Creature::VOICE_GAP_MAX)(m_rng);
        m_occlusionTimer = 0.0f;   // occlusion evaluee des le debut de la ligne
    }

    // Etouffee derriere un mur (meme test de segment que la vue de fropy), reevalue periodiquement.
    // Chaque ligne a son propre filtre : on l'applique a la ligne en cours a chaque evaluation.
    m_occlusionTimer -= dt;
    if (m_current && collisionManager && m_occlusionTimer <= 0.0f) {
        m_occlusionTimer = Constants::Creature::VOICE_OCCLUSION_INTERVAL;
        const bool occluded = glm::distance(mouth, listenerPos) < Constants::Creature::VOICE_MAX_DISTANCE &&
                              collisionManager->segmentBlocked(mouth, listenerPos);
        m_current->setLowPassFilter(1.0f, occluded ? Constants::Creature::VOICE_OCCLUDED_HF : 1.0f);
    }
}
