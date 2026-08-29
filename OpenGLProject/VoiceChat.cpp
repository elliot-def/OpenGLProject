#include "VoiceChat.h"

#include "SteamManager.h"
#include "Log.h"

// OpenAL (capture + lecture)
#include <AL/al.h>
#include <AL/alc.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

// ── IMA ADPCM (codec 4:1, spec classique) ─────────────────────────────────
constexpr int kImaStepTable[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
    3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};
constexpr int kImaIndexTable[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8
};

// Encode `n` echantillons PCM (n doit etre pair) en IMA ADPCM. `predictor` et
// `stepIndex` sont l'etat initial DU B LOC (stocke dans le paquet).
void encodeIma(const int16_t* pcm, int n, int16_t& predictor, uint8_t& stepIndex,
               uint8_t* out) {
    int pred = predictor;
    int index = stepIndex;
    int outIdx = 0;
    for (int i = 0; i < n; i += 2) {
        // Deux echantillons par octet : sample i dans le nibble bas, i+1 haut.
        int codes = 0;
        for (int s = 0; s < 2; s++) {
            const int sample = pcm[i + s];
            const int step = kImaStepTable[index];
            int diff = sample - pred;
            int code = 0;
            if (diff < 0) { code = 8; diff = -diff; }
            if (diff >= step) { code |= 4; diff -= step; }
            int stepHalf = step >> 1;
            if (diff >= stepHalf) { code |= 2; diff -= stepHalf; }
            stepHalf >>= 1;
            if (diff >= stepHalf) { code |= 1; }

            // Reconstruction du predictor (meme formule que le decodeur).
            int diffq = step >> 3;
            if (code & 4) diffq += step;
            if (code & 2) diffq += step >> 1;
            if (code & 1) diffq += step >> 2;
            if (code & 8) diffq = -diffq;
            pred += diffq;
            if (pred > 32767) pred = 32767;
            else if (pred < -32768) pred = -32768;

            index += kImaIndexTable[code & 7];
            if (index < 0) index = 0;
            else if (index > 88) index = 88;

            codes |= (s == 0) ? code : (code << 4);
        }
        out[outIdx++] = static_cast<uint8_t>(codes);
    }
    predictor = static_cast<int16_t>(pred);
    stepIndex = static_cast<uint8_t>(index);
}

void decodeIma(const uint8_t* in, int numCodes, int16_t& predictor, uint8_t& stepIndex,
               int16_t* out) {
    int pred = predictor;
    int index = stepIndex;
    int outIdx = 0;
    for (int c = 0; c < numCodes; c++) {
        const uint8_t byte = in[c];
        for (int nib = 0; nib < 2; nib++) {
            const int code = (nib == 0) ? (byte & 0x0F) : ((byte >> 4) & 0x0F);
            const int step = kImaStepTable[index];
            int diffq = step >> 3;
            if (code & 4) diffq += step;
            if (code & 2) diffq += step >> 1;
            if (code & 1) diffq += step >> 2;
            if (code & 8) diffq = -diffq;
            pred += diffq;
            if (pred > 32767) pred = 32767;
            else if (pred < -32768) pred = -32768;
            index += kImaIndexTable[code & 7];
            if (index < 0) index = 0;
            else if (index > 88) index = 88;
            out[outIdx++] = static_cast<int16_t>(pred);
        }
    }
    predictor = static_cast<int16_t>(pred);
    stepIndex = static_cast<uint8_t>(index);
}

// Seuil VAD : sensitivity 0..1 -> RMS 0.001 (tres sensible) .. 0.5 (fort).
float vadThreshold(float sensitivity) {
    const float s = std::clamp(sensitivity, 0.0f, 1.0f);
    return 0.001f * std::pow(1000.0f, s);
}

} // namespace

// ---------------------------------------------------------------------------
// Stream de lecture d'un joueur distant (source OpenAL + pool de buffers)
// ---------------------------------------------------------------------------
struct VoiceChat::Stream {
    unsigned int source = 0;           // ALuint
    std::vector<unsigned int> buffers; // ALuint, buffers actuellement en file (FIFO)
    bool playing = false;

    ~Stream() {
        if (source != 0) {
            alSourceStop(source);
            alDeleteSources(1, &source);
        }
        if (!buffers.empty()) {
            alDeleteBuffers(static_cast<ALsizei>(buffers.size()), buffers.data());
        }
    }
};

// ---------------------------------------------------------------------------
// Vie
// ---------------------------------------------------------------------------

VoiceChat::VoiceChat(SteamManager* steam, GLFWwindow* window)
    : m_steam(steam), m_window(window) {
}

VoiceChat::~VoiceChat() {
    shutdown();
}

void VoiceChat::shutdown() {
    closeCapture();
    releaseStreams();
}

void VoiceChat::onLobbyChanged() {
    m_inLobby = true;
    releaseStreams();
}

void VoiceChat::onLobbyLeft() {
    m_inLobby = false;
    closeCapture();
    releaseStreams();
}

// ---------------------------------------------------------------------------
// Devices de capture (pour le select du menu Audio)
// ---------------------------------------------------------------------------

std::vector<std::string> VoiceChat::getCaptureDevices() {
    std::vector<std::string> devices;
    devices.emplace_back("Defaut (systeme)");
    const ALCchar* list = alcGetString(nullptr, ALC_CAPTURE_DEVICE_SPECIFIER);
    if (list) {
        while (list[0] != '\0') {
            devices.emplace_back(list);
            list += std::strlen(list) + 1;
        }
    }
    return devices;
}

std::string VoiceChat::cleanDeviceName(const std::string& raw) {
    // OpenAL Soft prefixe les devices par "OpenAL Soft on ". On le retire
    // pour n'afficher que le nom lisible dans le select du menu Audio.
    const std::string prefix = "OpenAL Soft on ";
    if (raw.rfind(prefix, 0) == 0) {
        return raw.substr(prefix.size());
    }
    return raw;
}

int VoiceChat::pttKey() {
    return GLFW_KEY_V;
}

// ---------------------------------------------------------------------------
// Capture
// ---------------------------------------------------------------------------

void VoiceChat::openCapture() {
    if (m_capture) return;

    const std::string& dev = m_settings.device;
    const char* devName = dev.empty() ? nullptr : dev.c_str();
    ALCdevice* devCapture = alcCaptureOpenDevice(devName, SAMPLE_RATE,
                                                 AL_FORMAT_MONO16, CAPTURE_LATENCY);
    if (!devCapture) {
        LOG_WARN("[VoiceChat] Impossible d'ouvrir le device de capture '%s' (fallback defaut).",
                 devName ? devName : "(defaut)");
        devCapture = alcCaptureOpenDevice(nullptr, SAMPLE_RATE, AL_FORMAT_MONO16,
                                          CAPTURE_LATENCY);
    }
    if (!devCapture) {
        LOG_ERROR("[VoiceChat] Aucun micro disponible, chat vocal inactif.");
        return;
    }
    alcCaptureStart(devCapture);
    m_capture = devCapture;
    m_openDevice = m_settings.device;
    m_seq = 0;
    m_speechHangover = 0;
    m_transmitting = false;
    // Nouvelle session de capture : l'etat ADPCM repart de silence (il sera
    // porte d'un bloc a l'autre ensuite, voir encodeAndSend).
    m_predictor = 0;
    m_stepIndex = 0;
    LOG_INFO("[VoiceChat] Capture ouverte (%u Hz mono16, device='%s').",
             static_cast<unsigned>(SAMPLE_RATE), devName ? devName : "(defaut)");
}

void VoiceChat::closeCapture() {
    if (!m_capture) return;
    ALCdevice* dev = static_cast<ALCdevice*>(m_capture);
    alcCaptureStop(dev);
    alcCaptureCloseDevice(dev);
    m_capture = nullptr;
    m_transmitting = false;
}

// ---------------------------------------------------------------------------
// Envoi
// ---------------------------------------------------------------------------

float VoiceChat::computeRms(const int16_t* pcm, uint32_t numSamples) {
    double sum = 0.0;
    for (uint32_t i = 0; i < numSamples; i++) {
        const double s = static_cast<double>(pcm[i]);
        sum += s * s;
    }
    if (numSamples == 0) return 0.0f;
    return static_cast<float>(std::sqrt(sum / static_cast<double>(numSamples)) / 32768.0);
}

void VoiceChat::encodeAndSend(const int16_t* pcm, uint32_t numSamples) {
    if (!m_steam || !m_steam->isInLobby()) return;

    VoicePacket pkt;
    std::memset(&pkt, 0, sizeof(pkt));
    std::memcpy(pkt.magic, "MPGV", 4);
    pkt.version = 1;
    pkt.seq = m_seq++;
    // Etat ADPCM de DEBUT de bloc (= fin du bloc precedent) : le recepteur
    // decode chaque bloc independamment depuis cet etat, mais l'encodeur
    // continue la prediction entre blocs (sinon clic a chaque frontiere).
    pkt.predictor = m_predictor;
    pkt.stepIndex = m_stepIndex;
    encodeIma(pcm, static_cast<int>(numSamples), m_predictor, m_stepIndex,
              pkt.data);

    m_steam->broadcastP2PUnreliable(&pkt, sizeof(pkt));
}

// ---------------------------------------------------------------------------
// Reception / lecture
// ---------------------------------------------------------------------------

void VoiceChat::onVoicePacket(uint64_t senderID, const uint8_t* data, uint32_t size) {
    if (!m_settings.enabled || !m_inLobby) return;
    if (size != sizeof(VoicePacket)) return;

    VoicePacket pkt;
    std::memcpy(&pkt, data, sizeof(pkt));
    if (std::memcmp(pkt.magic, "MPGV", 4) != 0 || pkt.version != 1) return;

    // Creer le stream du joueur a la reception de son premier paquet.
    auto& streamPtr = m_streams[senderID];
    if (!streamPtr) {
        auto stream = std::make_unique<Stream>();
        alGenSources(1, &stream->source);
        // IMPORTANT : source relative -> pas d'attenuation par distance
        // (le modele de distance du SoundManager s'appliquerait sinon).
        alSourcei(stream->source, AL_SOURCE_RELATIVE, AL_TRUE);
        alSourcef(stream->source, AL_GAIN, m_settings.volume);
        alSourcef(stream->source, AL_PITCH, 1.0f);
        streamPtr = std::move(stream);
    }
    Stream& s = *streamPtr;

    // Recycler les buffers deja joues (FIFO) : les retirer de la source et
    // du pool, puis les liberer. Sans cela, ils restaient en file et etaient
    // rejoues a l'infini une fois la source arretee (voir update()).
    ALint processed = 0;
    alGetSourcei(s.source, AL_BUFFERS_PROCESSED, &processed);
    for (ALint i = 0; i < processed; ++i) {
        ALuint buf = 0;
        alSourceUnqueueBuffers(s.source, 1, &buf);
        if (buf != 0) alDeleteBuffers(1, &buf);
        if (!s.buffers.empty()) s.buffers.erase(s.buffers.begin());
    }

    // Si toute la pool est encore en file (lecture en retard), on jette le
    // paquet : mieux vaut un trou qu'un delai qui grossit.
    if (s.buffers.size() >= static_cast<size_t>(STREAM_BUFFERS)) {
        return;
    }

    // Decoder le bloc PCM 16k mono16 et le pousser dans la file OpenAL.
    int16_t pcm[FRAME_SAMPLES] = {};
    int16_t predictor = pkt.predictor;
    uint8_t stepIndex = pkt.stepIndex;
    decodeIma(pkt.data, sizeof(pkt.data), predictor, stepIndex, pcm);

    ALuint buf = 0;
    alGenBuffers(1, &buf);
    alBufferData(buf, AL_FORMAT_MONO16, pcm, FRAME_BYTES, SAMPLE_RATE);
    alSourceQueueBuffers(s.source, 1, &buf);
    s.buffers.push_back(buf);

    ALint state = AL_STOPPED;
    alGetSourcei(s.source, AL_SOURCE_STATE, &state);
    if (state != AL_PLAYING) {
        alSourcePlay(s.source);
    }
}

// ---------------------------------------------------------------------------
// Update : capture + VAD/PTT + envoi, et bookkeeping de lecture
// ---------------------------------------------------------------------------

void VoiceChat::update(float dt) {
    (void)dt;

    // ── Gestion du device de capture ──
    const bool wantCapture = m_settings.enabled && m_inLobby && !m_settings.micMuted;
    if (wantCapture && !m_capture) {
        openCapture();
    } else if (!wantCapture && m_capture) {
        closeCapture();
    } else if (m_capture && m_openDevice != m_settings.device) {
        closeCapture();   // device change dans les options : reouverture
        if (wantCapture) openCapture();
    }

    // ── Bookkeeping des streams (recyclage + reprendre si arrete) ──
    for (auto it = m_streams.begin(); it != m_streams.end();) {
        Stream& s = *it->second;
        alSourcef(s.source, AL_GAIN, m_settings.volume);
        ALint state = AL_STOPPED;
        alGetSourcei(s.source, AL_SOURCE_STATE, &state);
        if (state != AL_PLAYING) {
            // Une source arretee = sous-approvisionnement (tous les buffers
            // ont ete joues). Les buffers joues restent marques "processed"
            // tant qu'ils ne sont pas dequeues : rejouer la source ici ferait
            // tourner le premier son en boucle. On ne reprend que s'il reste
            // des buffers NON joues, sinon on libere le stream.
            ALint queued = 0, processed = 0;
            alGetSourcei(s.source, AL_BUFFERS_QUEUED, &queued);
            alGetSourcei(s.source, AL_BUFFERS_PROCESSED, &processed);
            if (processed < queued) {
                alSourcePlay(s.source);  // reste des buffers non joues
            } else {
                // Plus rien a jouer : liberer le stream.
                it = m_streams.erase(it);
                continue;
            }
        }
        ++it;
    }

    // ── Capture + transmission ──
    if (!m_capture) return;

    const bool pttHeld = !m_settings.voiceActivation
                         && glfwGetKey(m_window, pttKey()) == GLFW_PRESS;

    ALCint available = 0;
    alcGetIntegerv(static_cast<ALCdevice*>(m_capture), ALC_CAPTURE_SAMPLES, 1,
                   &available);

    const float threshold = vadThreshold(m_settings.sensitivity);
    int16_t pcm[FRAME_SAMPLES];

    while (available >= static_cast<ALCint>(FRAME_SAMPLES)) {
        alcCaptureSamples(static_cast<ALCdevice*>(m_capture), pcm, FRAME_SAMPLES);
        available -= static_cast<ALCint>(FRAME_SAMPLES);

        // VAD : RMS du bloc + hysteresis (maintien ~60 ms apres la parole).
        bool transmit = pttHeld;
        if (m_settings.voiceActivation) {
            const float rms = computeRms(pcm, FRAME_SAMPLES);
            if (rms >= threshold) {
                m_speechHangover = HANGOVER_FRAMES;
            } else if (m_speechHangover > 0) {
                --m_speechHangover;
            }
            transmit = (rms >= threshold || m_speechHangover > 0);
        }

        m_transmitting = transmit;
        if (transmit) {
            encodeAndSend(pcm, FRAME_SAMPLES);
        }
    }
}

// ---------------------------------------------------------------------------
// Nettoyage
// ---------------------------------------------------------------------------

void VoiceChat::releaseStreams() {
    m_streams.clear();
}
