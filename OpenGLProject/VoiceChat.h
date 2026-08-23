#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct GLFWwindow;
class SteamManager;

// Paquet voix (canal P2P 0, NON-fiable) : 20 ms de PCM 16 kHz mono 16 bits
// compresse IMA ADPCM (4:1) + etat initial du codeur (chaque bloc se decode
// independamment, les pertes/desordres ne corrompent pas la suite).
#pragma pack(push, 1)
struct VoicePacket {
    char     magic[4];    // "MPGV"
    uint32_t version;     // 1
    uint32_t seq;         // sequence de l'emetteur (detection de pertes)
    int16_t  predictor;   // etat IMA ADPCM initial
    uint8_t  stepIndex;   // etat IMA ADPCM initial
    uint8_t  data[160];   // 320 echantillons codes IMA ADPCM (4 bits par echantillon)
};
#pragma pack(pop)
static_assert(sizeof(VoicePacket) == 175, "VoicePacket doit faire 175 octets");

// ---------------------------------------------------------------------------
// VoiceChat : chat vocal du lobby Steam.
//
// Capture : OpenAL (alcCaptureOpenDevice), 16 kHz mono 16 bits, traitee sur le
// thread principal par blocs de 20 ms (aucun thread supplementaire).
// Transmission : paquets NON-fiables sur le meme canal P2P que l'etat des
// joueurs (magic "MPGV"), recus par MultiplayerManager et transmis ici.
// Lecture : une source OpenAL par joueur distant, alimentee par une file de
// buffers (IMPORTANT : AL_SOURCE_RELATIVE = true, sinon le modele de distance
// AL_INVERSE_DISTANCE_CLAMPED de SoundManager attenu les voix).
//
// Mode d'activation : detection vocale (VAD, seuil reglable) OU push-to-talk
// (touche V). Reglages persistes dans res/options.json (section "voice").
// ---------------------------------------------------------------------------
class VoiceChat {
public:
    VoiceChat(SteamManager* steam, GLFWwindow* window);
    ~VoiceChat();

    struct Settings {
        bool enabled = false;        // chat vocal active
        bool micMuted = false;       // micro coupe (on n'envoie rien)
        bool voiceActivation = true; // true = VAD, false = push-to-talk (touche V)
        float sensitivity = 0.15f;   // seuil VAD (0..1, 1 = il faut parler fort)
        float volume = 1.0f;         // volume des voix des autres joueurs (0..1)
        std::string device;          // device de capture ("" = defaut systeme)
    };
    Settings& settings() { return m_settings; }
    const Settings& settings() const { return m_settings; }

    // Touche push-to-talk (fixe pour l'instant) + son libelle.
    static int pttKey();
    static constexpr const char* PTT_KEY_NAME = "V";

    // Liste des devices de capture disponibles (pour le select du menu Audio).
    // Premier element : "Defaut (systeme)".
    static std::vector<std::string> getCaptureDevices();

    // Retire le prefixe "OpenAL Soft on " d'un nom de device pour l'affichage
    // (ex: "Virtual Input (Rode Connect)" au lieu de "OpenAL Soft on ...").
    static std::string cleanDeviceName(const std::string& raw);

    // Paquet voix recu d'un pair (transmis par MultiplayerManager).
    void onVoicePacket(uint64_t senderID, const uint8_t* data, uint32_t size);

    // Cycle de vie du lobby.
    void onLobbyChanged();   // lobby rejoint : pret a parler
    void onLobbyLeft();      // lobby quitte : coupe capture + voix

    // À appeler chaque frame en jeu (STATE_PLAYING, dans un lobby).
    void update(float dt);

    // Ferme la capture + coupe les voix (pause, menu, options).
    void shutdown();

private:
    // Source OpenAL d'un joueur distant (defini dans le .cpp pour ne pas
    // exposer les types OpenAL ici).
    struct Stream;

    void openCapture();
    void closeCapture();
    void releaseStreams();   // stop + detruit les sources OpenAL des pairs
    void encodeAndSend(const int16_t* pcm, uint32_t numSamples);
    static float computeRms(const int16_t* pcm, uint32_t numSamples);

    SteamManager* m_steam = nullptr;
    GLFWwindow* m_window = nullptr;
    Settings m_settings;
    bool m_inLobby = false;

    // Capture OpenAL (device ALC, independant du device de lecture)
    void* m_capture = nullptr;   // ALCdevice*
    std::string m_openDevice;    // device utilise a l'ouverture (pour detecter
                                 // un changement dans les reglages)
    uint32_t m_seq = 0;          // sequence locale des paquets envoyes
    int m_speechHangover = 0;    // maintien de la transmission apres la parole
    bool m_transmitting = false; // etat courant (pour logs/events futurs)

    // Streams de lecture par joueur distant (SteamID -> source OpenAL)
    std::map<uint64_t, std::unique_ptr<Stream>> m_streams;

    static constexpr uint32_t SAMPLE_RATE = 16000;    // Hz
    static constexpr uint32_t FRAME_SAMPLES = 320;    // 20 ms
    static constexpr uint32_t FRAME_BYTES = FRAME_SAMPLES * 2;
    static constexpr uint32_t CAPTURE_LATENCY = FRAME_SAMPLES * 4;
    static constexpr int HANGOVER_FRAMES = 3;         // ~60 ms apres la parole
    static constexpr int STREAM_BUFFERS = 8;          // ~160 ms de latence max
};
