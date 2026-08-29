#include "Log.h"
#include "MusicStream.h"

#include <AL/al.h>

#include <fstream>
#include <iostream>

// Déclarations seulement (l'implémentation de stb_vorbis est compilée à part,
// dans le fichier C dependencies/stb/stb_vorbis.c ajouté au projet).
#define STB_VORBIS_HEADER_ONLY
#include <stb/stb_vorbis.c>

// ─── Constructeur / Destructeur ───────────────────────────────────────────────

MusicStream::MusicStream(const std::string& filePath, float gain)
    : m_filePath(filePath), m_gain(gain)
{
    // Lire tout le fichier compressé en mémoire. Un .ogg pèse quelques Mo
    // (contre ~35 Mo de PCM pour le .wav), donc c'est rapide et non bloquant.
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        logErr() << "[MusicStream] Impossible d'ouvrir : " << filePath << std::endl;
        return;
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    if (size <= 0) {
        logErr() << "[MusicStream] Fichier vide : " << filePath << std::endl;
        return;
    }
    m_data.resize(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(m_data.data()), size);

    int error = 0;
    m_stream = stb_vorbis_open_memory(m_data.data(), static_cast<int>(m_data.size()),
                                      &error, nullptr);
    if (!m_stream) {
        logErr() << "[MusicStream] Decodage Vorbis impossible (erreur " << error
                  << ") : " << filePath << std::endl;
        return;
    }

    stb_vorbis_info info = stb_vorbis_get_info(m_stream);
    m_sampleRate = static_cast<int>(info.sample_rate);
    m_channels = info.channels > 2 ? 2 : info.channels;  // stéréo max
    m_format = (m_channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

    alGenSources(1, &m_source);
    m_buffers.resize(BUFFER_COUNT);
    alGenBuffers(static_cast<ALsizei>(m_buffers.size()), m_buffers.data());

    m_chunk.resize(static_cast<size_t>(CHUNK_SAMPLES) * m_channels);

    alSourcei(m_source, AL_LOOPING, AL_FALSE);  // boucle gérée manuellement
    alSourcef(m_source, AL_GAIN, m_gain);
    // Musique de fond : source relative à l'auditeur (pas d'atténuation 3D).
    alSourcei(m_source, AL_SOURCE_RELATIVE, AL_TRUE);

    m_ready = true;

    logOut() << "[MusicStream] Flux audio pret : " << filePath
              << " (" << m_sampleRate << " Hz, " << m_channels << " canal(aux))"
              << std::endl;
}

MusicStream::~MusicStream() {
    stop();
    if (m_source) {
        alDeleteSources(1, &m_source);
        m_source = 0;
    }
    if (!m_buffers.empty()) {
        alDeleteBuffers(static_cast<ALsizei>(m_buffers.size()), m_buffers.data());
        m_buffers.clear();
    }
    if (m_stream) {
        stb_vorbis_close(m_stream);
        m_stream = nullptr;
    }
}

// ─── Lecture ──────────────────────────────────────────────────────────────────

void MusicStream::play() {
    if (!m_ready) return;

    ALint state = 0;
    alGetSourcei(m_source, AL_SOURCE_STATE, &state);
    if (state == AL_PLAYING) return;

    if (state == AL_PAUSED) {
        alSourcePlay(m_source);  // simple reprise
        return;
    }

    // STOPPED / INITIAL : (re)démarrer depuis le début.
    stb_vorbis_seek_start(m_stream);
    fillInitialQueue();
    alSourcePlay(m_source);
}

void MusicStream::pause() {
    if (!m_ready) return;
    alSourcePause(m_source);
}

void MusicStream::resume() {
    if (!m_ready) return;
    if (isPaused())
        alSourcePlay(m_source);
}

void MusicStream::stop() {
    if (!m_ready) return;
    clearQueue();
    stb_vorbis_seek_start(m_stream);
}

bool MusicStream::isPlaying() const {
    if (!m_ready) return false;
    ALint state = 0;
    alGetSourcei(m_source, AL_SOURCE_STATE, &state);
    return state == AL_PLAYING;
}

bool MusicStream::isPaused() const {
    if (!m_ready) return false;
    ALint state = 0;
    alGetSourcei(m_source, AL_SOURCE_STATE, &state);
    return state == AL_PAUSED;
}

void MusicStream::setGain(float gain) {
    m_gain = gain;
    if (m_ready)
        alSourcef(m_source, AL_GAIN, gain);
}

void MusicStream::setLoop(bool loop) {
    m_loop = loop;
}

// ─── Streaming ────────────────────────────────────────────────────────────────

bool MusicStream::decodeChunkInto(ALuint buffer) {
    int count = stb_vorbis_get_samples_short_interleaved(
        m_stream, m_channels, m_chunk.data(), static_cast<int>(m_chunk.size()));
    if (count <= 0)
        return false;  // fin de flux

    ALsizei bytes = static_cast<ALsizei>(count * m_channels * static_cast<int>(sizeof(short)));
    alBufferData(buffer, m_format, m_chunk.data(), bytes, m_sampleRate);
    return true;
}

void MusicStream::clearQueue() {
    if (!m_source) return;
    alSourceStop(m_source);

    // alSourceUnqueueBuffers ne retire QUE les buffers déjà "traités" (joués).
    // Pour vider aussi les buffers encore en attente, on détache tout via
    // AL_BUFFER = 0 : c'est la seule façon fiable de remettre la file à zéro.
    alSourcei(m_source, AL_BUFFER, 0);
}

void MusicStream::fillInitialQueue() {
    clearQueue();
    for (ALuint buf : m_buffers) {
        if (!decodeChunkInto(buf))
            break;
        alSourceQueueBuffers(m_source, 1, &buf);
    }
}

void MusicStream::update() {
    if (!m_ready) return;

    ALint processed = 0;
    alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &processed);

    while (processed-- > 0) {
        ALuint buf = 0;
        alSourceUnqueueBuffers(m_source, 1, &buf);

        if (!decodeChunkInto(buf)) {
            // Fin du morceau : boucle ou arrêt définitif.
            if (!m_loop)
                continue;  // ne pas ré-empiler → la source s'arrête d'elle-même
            stb_vorbis_seek_start(m_stream);
            if (!decodeChunkInto(buf))
                continue;
        }
        alSourceQueueBuffers(m_source, 1, &buf);
    }

    // Anti sous-alimentation : si la source s'est arrêtée alors qu'il reste des
    // buffers en file (ralentissement ponctuel), on relance la lecture.
    ALint state = 0;
    alGetSourcei(m_source, AL_SOURCE_STATE, &state);
    if (state == AL_STOPPED) {
        ALint queued = 0;
        alGetSourcei(m_source, AL_BUFFERS_QUEUED, &queued);
        if (queued > 0)
            alSourcePlay(m_source);
    }
}
