#pragma comment(lib, "OpenAL32.lib")

#include "SoundManager.h"
#include "Sound.h"


#include <AL/al.h>
#include <AL/alc.h>
#include <AL/efx.h>
#include <AL/alext.h>
#include <AL/efx-presets.h>   // EFXEAXREVERBPROPERTIES presets

// Définitions de secours pour macros de preset éventuellement absentes dans certaines implémentations
#ifndef EFX_REVERB_PRESET_OUTDOORS_DEEPVALLEY
  #if defined(EFX_REVERB_PRESET_PLAIN)
    #define EFX_REVERB_PRESET_OUTDOORS_DEEPVALLEY EFX_REVERB_PRESET_PLAIN
  #else
    #define EFX_REVERB_PRESET_OUTDOORS_DEEPVALLEY EFX_REVERB_PRESET_GENERIC
  #endif
#endif

#include <stdexcept>
#include <iostream>
#include <algorithm>

// ─── Helpers ─────────────────────────────────────────────────────────────────

static void alcCheckError(ALCdevice* device, const char* context) {
    ALCenum err = alcGetError(device);
    if (err != ALC_NO_ERROR) {
        std::cerr << "[OpenAL ALC] Erreur dans " << context
            << " : 0x" << std::hex << err << std::dec << std::endl;
    }
}

// ─── Constructeur / Destructeur ───────────────────────────────────────────────

SoundManager::SoundManager() {
    initOpenAL();
}

SoundManager::~SoundManager() {
    // Supprimer tous les effect slots
    for (unsigned int slot : m_effectSlots)
        destroyReverbEffect(slot);
    m_effectSlots.clear();

    unloadAll();
    shutdownOpenAL();
}

// ─── Initialisation OpenAL ────────────────────────────────────────────────────

void SoundManager::initOpenAL() {
    // Ouvrir le périphérique audio par défaut
    m_device = alcOpenDevice(nullptr);
    if (!m_device)
        throw std::runtime_error("[SoundManager] Impossible d'ouvrir le périphérique OpenAL.");

    // Créer le contexte avec la liste d'attributs pour activer EFX
    ALint attrs[] = {
        ALC_MAX_AUXILIARY_SENDS, 4,  // 4 envois auxiliaires pour les effets
        0
    };
    m_context = alcCreateContext(m_device, attrs);
    alcCheckError(m_device, "alcCreateContext");

    if (!alcMakeContextCurrent(m_context)) {
        alcDestroyContext(m_context);
        alcCloseDevice(m_device);
        throw std::runtime_error("[SoundManager] Impossible d'activer le contexte OpenAL.");
    }

    // Vérifier la disponibilité de l'extension EFX
    m_efxAvailable = alcIsExtensionPresent(m_device, "ALC_EXT_EFX") == ALC_TRUE;

    if (m_efxAvailable)
        std::cout << "[SoundManager] Extension EFX disponible (reverb, filtres actifs)." << std::endl;
    else
        std::cout << "[SoundManager] Extension EFX absente (reverb/filtres désactivés)." << std::endl;

    // Modèle d'atténuation par défaut : distance inverse clampée (réaliste)
    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

    std::cout << "[SoundManager] OpenAL initialisé. Vendor : "
        << alGetString(AL_VENDOR) << std::endl;
}

void SoundManager::shutdownOpenAL() {
    alcMakeContextCurrent(nullptr);
    if (m_context) {
        alcDestroyContext(m_context);
        m_context = nullptr;
    }
    if (m_device) {
        alcCloseDevice(m_device);
        m_device = nullptr;
    }
}

// ─── Chargement ──────────────────────────────────────────────────────────────

Sound* SoundManager::load(const std::string& name,
    const std::string& filePath,
    bool loop, float gain, float pitch)
{
    // Cache : si déjà chargé, retourner le son existant
    auto it = m_sounds.find(name);
    if (it != m_sounds.end())
        return it->second.get();

    auto sound = std::make_unique<Sound>(filePath, glm::vec3(0.0f), loop, gain, pitch);
    Sound* ptr = sound.get();
    m_sounds.emplace(name, std::move(sound));

    std::cout << "[SoundManager] Son chargé : \"" << name << "\" ← " << filePath << std::endl;
    return ptr;
}

void SoundManager::unload(const std::string& name) {
    auto it = m_sounds.find(name);
    if (it != m_sounds.end()) {
        it->second->stop();
        m_sounds.erase(it);
    }
}

void SoundManager::unloadAll() {
    for (auto& [name, sound] : m_sounds)
        sound->stop();
    m_sounds.clear();
}

Sound* SoundManager::get(const std::string& name) {
    auto it = m_sounds.find(name);
    if (it == m_sounds.end())
        throw std::out_of_range("[SoundManager] Son inconnu : " + name);
    return it->second.get();
}

// ─── Contrôle global ─────────────────────────────────────────────────────────

void SoundManager::setMasterVolume(float volume) {
    m_masterVolume = volume;
    if (!m_muted)
        alListenerf(AL_GAIN, volume);
}

void SoundManager::pauseAll() {
    for (auto& [name, sound] : m_sounds)
        if (sound->isPlaying())
            sound->pause();
}

void SoundManager::resumeAll() {
    for (auto& [name, sound] : m_sounds)
        if (sound->isPaused())
            sound->resume();
}

void SoundManager::stopAll() {
    for (auto& [name, sound] : m_sounds)
        sound->stop();
}

void SoundManager::setMute(bool mute) {
    m_muted = mute;
    alListenerf(AL_GAIN, mute ? 0.0f : m_masterVolume);
}

// ─── Listener ────────────────────────────────────────────────────────────────

void SoundManager::setListenerTransform(glm::vec3 position,
    glm::vec3 front,
    glm::vec3 up,
    glm::vec3 velocity)
{
    alListener3f(AL_POSITION, position.x, position.y, position.z);
    alListener3f(AL_VELOCITY, velocity.x, velocity.y, velocity.z);

    // OpenAL attend un tableau de 6 floats : [front (3)] + [up (3)]
    float orientation[6] = {
        front.x, front.y, front.z,
        up.x,    up.y,    up.z
    };
    alListenerfv(AL_ORIENTATION, orientation);
}

// ─── Doppler ─────────────────────────────────────────────────────────────────

void SoundManager::setDopplerFactor(float factor) {
    alDopplerFactor(factor);
}

void SoundManager::setSpeedOfSound(float speed) {
    alSpeedOfSound(speed);
}

// ─── Réverbération EFX ───────────────────────────────────────────────────────

unsigned int SoundManager::createReverbEffect(ReverbPreset preset) {
    if (!m_efxAvailable) {
        std::cerr << "[SoundManager] EFX non disponible, reverb ignorée." << std::endl;
        return 0;
    }

    // Récupérer les fonctions EFX
    auto alGenEffects = reinterpret_cast<LPALGENEFFECTS>(alGetProcAddress("alGenEffects"));
    auto alEffecti = reinterpret_cast<LPALEFFECTI>(alGetProcAddress("alEffecti"));
    auto alGenAuxiliaryEffectSlots = reinterpret_cast<LPALGENAUXILIARYEFFECTSLOTS>(
        alGetProcAddress("alGenAuxiliaryEffectSlots"));
    auto alAuxiliaryEffectSloti = reinterpret_cast<LPALAUXILIARYEFFECTSLOTI>(
        alGetProcAddress("alAuxiliaryEffectSloti"));

    if (!alGenEffects || !alEffecti || !alGenAuxiliaryEffectSlots || !alAuxiliaryEffectSloti) {
        std::cerr << "[SoundManager] Fonctions EFX introuvables." << std::endl;
        return 0;
    }

    // Créer un effet EAX Reverb
    unsigned int effect = 0;
    alGenEffects(1, &effect);
    alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);

    applyReverbPreset(effect, preset);

    // Attacher l'effet à un auxiliary effect slot
    unsigned int slot = 0;
    alGenAuxiliaryEffectSlots(1, &slot);
    alAuxiliaryEffectSloti(slot, AL_EFFECTSLOT_EFFECT, static_cast<ALint>(effect));

    // L'effet peut être supprimé une fois attaché au slot
    auto alDeleteEffects = reinterpret_cast<LPALDELETEEFFECTS>(alGetProcAddress("alDeleteEffects"));
    if (alDeleteEffects)
        alDeleteEffects(1, &effect);

    m_effectSlots.push_back(slot);
    m_currentReverbSlot = slot;

    std::cout << "[SoundManager] Reverb slot créé (id=" << slot << ")." << std::endl;
    return slot;
}

void SoundManager::destroyReverbEffect(unsigned int effectSlot) {
    if (effectSlot == 0) return;

    auto alDeleteAuxiliaryEffectSlots = reinterpret_cast<LPALDELETEAUXILIARYEFFECTSLOTS>(
        alGetProcAddress("alDeleteAuxiliaryEffectSlots"));
    if (alDeleteAuxiliaryEffectSlots)
        alDeleteAuxiliaryEffectSlots(1, &effectSlot);

    m_effectSlots.erase(
        std::remove(m_effectSlots.begin(), m_effectSlots.end(), effectSlot),
        m_effectSlots.end());
}

/**
 * Remplit les paramètres EAX du preset choisi.
 * On utilise les macros EFXEAXREVERB_PRESET_* définies dans <AL/efx-presets.h>.
 */
void SoundManager::applyReverbPreset(unsigned int effect, ReverbPreset preset) {
    auto alEffectf = reinterpret_cast<LPALEFFECTF>(alGetProcAddress("alEffectf"));
    auto alEffectfv = reinterpret_cast<LPALEFFECTFV>(alGetProcAddress("alEffectfv"));
    auto alEffecti = reinterpret_cast<LPALEFFECTI>(alGetProcAddress("alEffecti"));

    if (!alEffectf || !alEffectfv || !alEffecti) return;

    // Sélectionner les données du preset
    EFXEAXREVERBPROPERTIES props;

    switch (preset) {
    case ReverbPreset::ROOM:
        props = EFX_REVERB_PRESET_ROOM;
        break;
    case ReverbPreset::HALLWAY:
        props = EFX_REVERB_PRESET_HALLWAY;
        break;
    case ReverbPreset::CAVE:
        props = EFX_REVERB_PRESET_CAVE;
        break;
    case ReverbPreset::ARENA:
        props = EFX_REVERB_PRESET_ARENA;
        break;
    case ReverbPreset::OUTDOORS:
        props = EFX_REVERB_PRESET_OUTDOORS_DEEPVALLEY;
        break;
    case ReverbPreset::UNDERWATER:
        props = EFX_REVERB_PRESET_UNDERWATER;
        break;
    case ReverbPreset::NONE:
    default:
        // Réverb neutre (décroissance quasi nulle)
        props = EFX_REVERB_PRESET_GENERIC;
        props.flDecayTime = 0.1f;
        // "flReverbGain" n'existe pas dans EFXEAXREVERBPROPERTIES.
        // Utiliser flGain (gain global) et flLateReverbGain (gain de la réverbération tardive).
        props.flGain = 0.0f;
        props.flLateReverbGain = 0.0f;
        break;
    }

    // Appliquer les paramètres sur l'effet
    alEffectf(effect, AL_EAXREVERB_DENSITY, props.flDensity);
    alEffectf(effect, AL_EAXREVERB_DIFFUSION, props.flDiffusion);
    alEffectf(effect, AL_EAXREVERB_GAIN, props.flGain);
    alEffectf(effect, AL_EAXREVERB_GAINHF, props.flGainHF);
    alEffectf(effect, AL_EAXREVERB_GAINLF, props.flGainLF);
    alEffectf(effect, AL_EAXREVERB_DECAY_TIME, props.flDecayTime);
    alEffectf(effect, AL_EAXREVERB_DECAY_HFRATIO, props.flDecayHFRatio);
    alEffectf(effect, AL_EAXREVERB_DECAY_LFRATIO, props.flDecayLFRatio);
    alEffectf(effect, AL_EAXREVERB_REFLECTIONS_GAIN, props.flReflectionsGain);
    alEffectf(effect, AL_EAXREVERB_REFLECTIONS_DELAY, props.flReflectionsDelay);
    alEffectf(effect, AL_EAXREVERB_LATE_REVERB_GAIN, props.flLateReverbGain);
    alEffectf(effect, AL_EAXREVERB_LATE_REVERB_DELAY, props.flLateReverbDelay);
    alEffectf(effect, AL_EAXREVERB_ECHO_TIME, props.flEchoTime);
    alEffectf(effect, AL_EAXREVERB_ECHO_DEPTH, props.flEchoDepth);
    alEffectf(effect, AL_EAXREVERB_MODULATION_TIME, props.flModulationTime);
    alEffectf(effect, AL_EAXREVERB_MODULATION_DEPTH, props.flModulationDepth);
    alEffectf(effect, AL_EAXREVERB_AIR_ABSORPTION_GAINHF, props.flAirAbsorptionGainHF);
    alEffectf(effect, AL_EAXREVERB_HFREFERENCE, props.flHFReference);
    alEffectf(effect, AL_EAXREVERB_LFREFERENCE, props.flLFReference);
    alEffectf(effect, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, props.flRoomRolloffFactor);
    alEffecti(effect, AL_EAXREVERB_DECAY_HFLIMIT, props.iDecayHFLimit);

    float reflPan[3] = { props.flReflectionsPan[0],
                         props.flReflectionsPan[1],
                         props.flReflectionsPan[2] };
    alEffectfv(effect, AL_EAXREVERB_REFLECTIONS_PAN, reflPan);

    float lateRevPan[3] = { props.flLateReverbPan[0],
                             props.flLateReverbPan[1],
                             props.flLateReverbPan[2] };
    alEffectfv(effect, AL_EAXREVERB_LATE_REVERB_PAN, lateRevPan);
}

void SoundManager::applyReverbToAll(ReverbPreset preset) {
    unsigned int slot = 0;

    if (preset != ReverbPreset::NONE) {
        slot = createReverbEffect(preset);
        if (slot == 0) return;  // EFX absent
    }

    for (auto& [name, sound] : m_sounds)
        sound->setReverbEffect(slot);

    // Détruire l'ancien slot global si besoin
    if (m_currentReverbSlot != 0 && m_currentReverbSlot != slot)
        destroyReverbEffect(m_currentReverbSlot);

    m_currentReverbSlot = slot;
}

// ─── Mise à jour ──────────────────────────────────────────────────────────────

void SoundManager::update() {
    // Rien à faire pour l'instant — OpenAL gère la lecture en interne.
    // On pourrait ici nettoyer les sons one-shot terminés si on les stockait
    // séparément (ex: vector<Sound*> m_oneShotSounds).
}

// ─── Debug ────────────────────────────────────────────────────────────────────

void SoundManager::printSoundList() const {
    std::cout << "=== Sons chargés (" << m_sounds.size() << ") ===" << std::endl;
    for (const auto& [name, sound] : m_sounds) {
        std::cout << "  [" << name << "] "
            << sound->getFilePath()
            << " | gain=" << sound->getGain()
            << " pitch=" << sound->getPitch()
            << " loop=" << (sound->isLooping() ? "oui" : "non")
            << " état=" << (sound->isPlaying() ? "lecture"
                : sound->isPaused() ? "pause"
                : "arrêté")
            << std::endl;
    }
}