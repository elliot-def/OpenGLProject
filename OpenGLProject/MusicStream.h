#pragma once

#include <string>
#include <vector>
#include <AL/al.h>

// Déclaration anticipée : le type complet vient de <stb/stb_vorbis.c>.
struct stb_vorbis;

/**
 * @class MusicStream
 * @brief Lecture en streaming d'une musique Ogg Vorbis
 *
 * Contrairement à Sound (qui décode tout le fichier en une seule fois dans un
 * buffer OpenAL), MusicStream décode le flux par petits morceaux et les empile
 * dans une file de buffers OpenAL (alSourceQueueBuffers). Le re-remplissage se
 * fait dans update(), appelée chaque frame par SoundManager.
 *
 * Conséquence : on ne garde jamais le PCM complet (~35 Mo) en mémoire, et il n'y
 * a pas de décodage bloquant au lancement de la lecture.
 *
 * Usage typique :
 * @code
 *   MusicStream music("res/sounds/on&on.ogg", 0.5f);
 *   music.play();
 *   // chaque frame :
 *   music.update();
 * @endcode
 */
class MusicStream {
public:
    /**
     * @brief Ouvre le fichier .ogg et prépare la source OpenAL.
     *
     * Lit uniquement le fichier compressé en mémoire (quelques Mo) : aucun
     * décodage PCM n'est fait ici, donc pas de freeze.
     *
     * @param filePath Chemin du fichier .ogg (Vorbis)
     * @param gain     Volume (1.0 = plein volume)
     */
    MusicStream(const std::string& filePath, float gain = 0.5f);
    ~MusicStream();

    MusicStream(const MusicStream&) = delete;
    MusicStream& operator=(const MusicStream&) = delete;

    /// @return true si le flux a été ouvert et décodé avec succès.
    bool isValid() const { return m_ready; }

    /// Reprend si en pause, sinon (re)démarre la lecture depuis le début.
    void play();
    void pause();
    void resume();
    /// Arrête la lecture et remet le flux au début.
    void stop();

    bool isPlaying() const;
    bool isPaused() const;

    void setGain(float gain);
    void setLoop(bool loop);

    /**
     * @brief À appeler chaque frame pour re-remplir les buffers consommés.
     *
     * Détache les buffers terminés, décode le morceau suivant et le ré-empile.
     * Gère aussi la boucle (retour au début) si setLoop(true).
     */
    void update();

private:
    /// Décode un morceau dans `buffer`. @return false en fin de flux.
    bool decodeChunkInto(ALuint buffer);
    /// Vide la file puis empile les premiers morceaux.
    void fillInitialQueue();
    /// Arrête la source et détache tous les buffers en file.
    void clearQueue();

    std::string m_filePath;

    // Fichier .ogg compressé complet en mémoire (petit : ~quelques Mo).
    std::vector<unsigned char> m_data;
    stb_vorbis* m_stream = nullptr;

    ALuint m_source = 0;
    std::vector<ALuint> m_buffers;  // file circulaire de buffers OpenAL
    std::vector<short>  m_chunk;    // buffer de décodage réutilisé

    ALenum m_format = AL_FORMAT_STEREO16;
    int    m_channels = 2;
    int    m_sampleRate = 44100;

    bool  m_ready = false;
    bool  m_loop = true;
    float m_gain = 0.5f;

    // 4 buffers × ~186 ms d'audio : assez de marge pour le re-remplissage par
    // frame, tout en gardant une empreinte mémoire minuscule.
    static constexpr int BUFFER_COUNT = 4;
    static constexpr int CHUNK_SAMPLES = 8192;  // échantillons par canal/morceau
};
