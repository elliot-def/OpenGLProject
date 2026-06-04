#include "Sound.h"

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/efx.h>          // Pour les filtres et effets (EFX)

#include <stdexcept>
#include <fstream>
#include <cstring>
#include <iostream>
#include <vector>

// ─── Helpers locaux ──────────────────────────────────────────────────────────

static void alCheckError(const char* context) {
    ALenum err = alGetError();
    if (err != AL_NO_ERROR) {
        std::cerr << "[OpenAL] Erreur dans " << context
            << " : 0x" << std::hex << err << std::dec << std::endl;
    }
}

// ─── Chargement WAV ──────────────────────────────────────────────────────────

/**
 * Lit un fichier .wav PCM (8 ou 16 bits, mono ou stéréo).
 * Format RIFF/WAVE uniquement — pas de compression.
 */
static bool loadWavFile(const std::string& path,
    std::vector<char>& outData,
    ALenum& outFormat,
    ALsizei& outFrequency)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[Sound] Impossible d'ouvrir : " << path << std::endl;
        return false;
    }

    // --- En-tête RIFF ---
    char riff[4];
    file.read(riff, 4);
    if (std::strncmp(riff, "RIFF", 4) != 0) {
        std::cerr << "[Sound] Pas un fichier RIFF : " << path << std::endl;
        return false;
    }

    uint32_t chunkSize;   file.read(reinterpret_cast<char*>(&chunkSize), 4);
    char wave[4];         file.read(wave, 4);
    if (std::strncmp(wave, "WAVE", 4) != 0) {
        std::cerr << "[Sound] Pas un fichier WAVE : " << path << std::endl;
        return false;
    }

    // --- Sous-chunk fmt ---
    char fmt[4];          file.read(fmt, 4);
    uint32_t fmtSize;     file.read(reinterpret_cast<char*>(&fmtSize), 4);
    uint16_t audioFmt;    file.read(reinterpret_cast<char*>(&audioFmt), 2);
    uint16_t channels;    file.read(reinterpret_cast<char*>(&channels), 2);
    uint32_t sampleRate;  file.read(reinterpret_cast<char*>(&sampleRate), 4);
    uint32_t byteRate;    file.read(reinterpret_cast<char*>(&byteRate), 4);
    uint16_t blockAlign;  file.read(reinterpret_cast<char*>(&blockAlign), 2);
    uint16_t bitsPerSample; file.read(reinterpret_cast<char*>(&bitsPerSample), 2);

    // Sauter les octets restants du chunk fmt si fmtSize > 16
    if (fmtSize > 16)
        file.seekg(fmtSize - 16, std::ios::cur);

    // --- Format OpenAL ---
    if (channels == 1 && bitsPerSample == 8)       outFormat = AL_FORMAT_MONO8;
    else if (channels == 1 && bitsPerSample == 16) outFormat = AL_FORMAT_MONO16;
    else if (channels == 2 && bitsPerSample == 8)  outFormat = AL_FORMAT_STEREO8;
    else if (channels == 2 && bitsPerSample == 16) outFormat = AL_FORMAT_STEREO16;
    else {
        std::cerr << "[Sound] Format non supporté (ch=" << channels
            << " bits=" << bitsPerSample << ") : " << path << std::endl;
        return false;
    }

    outFrequency = static_cast<ALsizei>(sampleRate);

    // --- Sous-chunk data ---
    char dataId[4];
    uint32_t dataSize = 0;

    // Parcourir les chunks jusqu'à trouver "data"
    while (file.read(dataId, 4)) {
        file.read(reinterpret_cast<char*>(&dataSize), 4);
        if (std::strncmp(dataId, "data", 4) == 0)
            break;
        file.seekg(dataSize, std::ios::cur);  // Ignorer les chunks LIST, INFO, etc.
        dataSize = 0;
    }

    if (dataSize == 0) {
        std::cerr << "[Sound] Chunk 'data' introuvable : " << path << std::endl;
        return false;
    }

    outData.resize(dataSize);
    file.read(outData.data(), dataSize);
    return true;
}

// ─── Constructeur / Destructeur ───────────────────────────────────────────────

Sound::Sound(const std::string& filePath,
    glm::vec3 position,
    bool loop,
    float gain,
    float pitch)
    : m_filePath(filePath),
    m_position(position),
    m_loop(loop),
    m_gain(gain),
    m_pitch(pitch)
{
    // Générer buffer et source
    alGenBuffers(1, &m_buffer);
    alCheckError("alGenBuffers");

    alGenSources(1, &m_source);
    alCheckError("alGenSources");

    loadWav(filePath);
    applyAll();
}

Sound::~Sound() {
    stop();

    // Détacher le filtre/effet avant de libérer la source
    if (m_filter != 0) {
        alSourcei(m_source, AL_DIRECT_FILTER, AL_FILTER_NULL);
        auto alDeleteFilters = reinterpret_cast<LPALDELETEFILTERS>(
            alGetProcAddress("alDeleteFilters"));
        if (alDeleteFilters)
            alDeleteFilters(1, &m_filter);
    }

    alDeleteSources(1, &m_source);
    alDeleteBuffers(1, &m_buffer);
}

// ─── Chargement ──────────────────────────────────────────────────────────────

void Sound::loadWav(const std::string& filePath) {
    std::vector<char> data;
    ALenum  format;
    ALsizei frequency;

    if (!loadWavFile(filePath, data, format, frequency)) {
        throw std::runtime_error("[Sound] Échec du chargement : " + filePath);
    }

    alBufferData(m_buffer,
        format,
        data.data(),
        static_cast<ALsizei>(data.size()),
        frequency);
    alCheckError("alBufferData");

    alSourcei(m_source, AL_BUFFER, static_cast<ALint>(m_buffer));
    alCheckError("alSourcei AL_BUFFER");
}

void Sound::applyAll() {
    alSourcef(m_source, AL_GAIN, m_gain);
    alSourcef(m_source, AL_PITCH, m_pitch);
    alSourcei(m_source, AL_LOOPING, m_loop ? AL_TRUE : AL_FALSE);
    alSource3f(m_source, AL_POSITION, m_position.x, m_position.y, m_position.z);
    alSource3f(m_source, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
    alSourcef(m_source, AL_REFERENCE_DISTANCE, m_referenceDistance);
    alSourcef(m_source, AL_MAX_DISTANCE, m_maxDistance);
    alSourcef(m_source, AL_ROLLOFF_FACTOR, m_rolloffFactor);
    alCheckError("Sound::applyAll");
}

// ─── Contrôle de lecture ──────────────────────────────────────────────────────

void Sound::play() {
    alSourcePlay(m_source);
    alCheckError("alSourcePlay");
}

void Sound::pause() {
    alSourcePause(m_source);
    alCheckError("alSourcePause");
}

void Sound::stop() {
    alSourceStop(m_source);
    alCheckError("alSourceStop");
}

void Sound::resume() {
    if (isPaused())
        alSourcePlay(m_source);
}

// ─── Paramètres de base ───────────────────────────────────────────────────────

void Sound::setGain(float gain) {
    m_gain = gain;
    alSourcef(m_source, AL_GAIN, gain);
    alCheckError("setGain");
}

void Sound::setPitch(float pitch) {
    m_pitch = pitch;
    alSourcef(m_source, AL_PITCH, pitch);
    alCheckError("setPitch");
}

void Sound::setLoop(bool loop) {
    m_loop = loop;
    alSourcei(m_source, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
    alCheckError("setLoop");
}

// ─── Spatialisation 3D ────────────────────────────────────────────────────────

void Sound::setPosition(glm::vec3 position) {
    m_position = position;
    alSource3f(m_source, AL_POSITION, position.x, position.y, position.z);
    alCheckError("setPosition");
}

void Sound::setVelocity(glm::vec3 velocity) {
    m_velocity = velocity;
    alSource3f(m_source, AL_VELOCITY, velocity.x, velocity.y, velocity.z);
    alCheckError("setVelocity");
}

void Sound::setDistanceModel(float referenceDistance,
    float maxDistance,
    float rolloffFactor) {
    m_referenceDistance = referenceDistance;
    m_maxDistance = maxDistance;
    m_rolloffFactor = rolloffFactor;

    alSourcef(m_source, AL_REFERENCE_DISTANCE, referenceDistance);
    alSourcef(m_source, AL_MAX_DISTANCE, maxDistance);
    alSourcef(m_source, AL_ROLLOFF_FACTOR, rolloffFactor);
    alCheckError("setDistanceModel");
}

// ─── Effets (EFX) ─────────────────────────────────────────────────────────────

void Sound::setLowPassFilter(float gainLF, float gainHF) {
    // Récupérer dynamiquement les fonctions EFX
    auto alGenFilters = reinterpret_cast<LPALGENFILTERS>(alGetProcAddress("alGenFilters"));
    auto alFilteri = reinterpret_cast<LPALFILTERI>(alGetProcAddress("alFilteri"));
    auto alFilterf = reinterpret_cast<LPALFILTERF>(alGetProcAddress("alFilterf"));

    if (!alGenFilters || !alFilteri || !alFilterf) {
        std::cerr << "[Sound] Extension EFX absente, filtre passe-bas ignoré." << std::endl;
        return;
    }

    if (m_filter == 0)
        alGenFilters(1, &m_filter);

    alFilteri(m_filter, AL_FILTER_TYPE, AL_FILTER_LOWPASS);
    alFilterf(m_filter, AL_LOWPASS_GAIN, gainLF);
    alFilterf(m_filter, AL_LOWPASS_GAINHF, gainHF);

    alSourcei(m_source, AL_DIRECT_FILTER, static_cast<ALint>(m_filter));
    alCheckError("setLowPassFilter");
}

void Sound::setReverbEffect(unsigned int effectSlot) {
    // AL_AUXILIARY_SEND_FILTER : (slot, send index, filter)
    auto alSource3i = reinterpret_cast<LPALSOURCE3I>(alGetProcAddress("alSource3i"));
    if (!alSource3i) {
        std::cerr << "[Sound] alSource3i introuvable, effet de réverb ignoré." << std::endl;
        return;
    }
    alSource3i(m_source,
        AL_AUXILIARY_SEND_FILTER,
        static_cast<ALint>(effectSlot),
        0,
        AL_FILTER_NULL);
    alCheckError("setReverbEffect");
}

// ─── Getters d'état ───────────────────────────────────────────────────────────

bool Sound::isPlaying() const {
    ALint state;
    alGetSourcei(m_source, AL_SOURCE_STATE, &state);
    return state == AL_PLAYING;
}

bool Sound::isPaused() const {
    ALint state;
    alGetSourcei(m_source, AL_SOURCE_STATE, &state);
    return state == AL_PAUSED;
}