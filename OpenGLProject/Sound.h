#pragma once

#include <string>
#include <glm/glm.hpp>

// Forward declarations
struct ALCdevice_struct;
struct ALCcontext_struct;

/**
 * @class Sound
 * @brief Représente une source sonore 3D dans l'espace de la scène
 *
 * Encapsule un buffer OpenAL (données audio) et une source OpenAL (émetteur 3D).
 * Supporte la spatialisation, l'atténuation par la distance, le pitch, le gain, etc.
 *
 * Analogie avec le reste du projet :
 *   Sound       <-> LightSource   (une entité sonore dans la scène)
 *   SoundManager <-> LightManager  (gère et met à jour toutes les sources)
 */
class Sound {
public:
    /**
     * @brief Constructeur — charge un fichier .wav et initialise la source OpenAL
     * @param filePath Chemin vers le fichier audio (.wav uniquement)
     * @param position Position 3D de la source dans la scène
     * @param loop     Lecture en boucle ou non
     * @param gain     Volume de la source (0.0 = muet, 1.0 = plein volume)
     * @param pitch    Hauteur tonale (1.0 = normale, < 1 grave, > 1 aigu)
     */
    Sound(const std::string& filePath,
        glm::vec3 position = glm::vec3(0.0f),
        bool loop = false,
        float gain = 1.0f,
        float pitch = 1.0f);

    ~Sound();

    // --- Contrôle de lecture ---

    void play();
    void pause();
    void stop();
    void resume();

    // --- Paramètres de base ---

    void setGain(float gain);          // Volume : 0.0 → ∞  (1.0 = référence)
    void setPitch(float pitch);        // Fréquence : 0.5 = moitié, 2.0 = double
    void setLoop(bool loop);

    // --- Spatialisation 3D ---

    void setPosition(glm::vec3 position);
    void setVelocity(glm::vec3 velocity);   // Pour l'effet Doppler

    /**
     * @brief Paramètres d'atténuation par la distance
     *
     * OpenAL utilise le modèle « Inverse Distance Clamped » par défaut :
     *   gain = referenceDistance / (referenceDistance + rolloffFactor
     *          * (clamp(distance, referenceDistance, maxDistance) - referenceDistance))
     *
     * @param referenceDistance Distance à partir de laquelle le son commence à s'atténuer
     * @param maxDistance       Distance au-delà de laquelle le son ne s'atténue plus
     * @param rolloffFactor     Vitesse d'atténuation (0 = pas d'atténuation, 1 = physique)
     */
    void setDistanceModel(float referenceDistance,
        float maxDistance,
        float rolloffFactor);

    // --- Effets de filtre (passe-bas) ---

    /**
     * @brief Applique un filtre passe-bas pour simuler un son assourdi / derrière un mur
     * @param gainLF Gain basses fréquences (0.0 → 1.0), 1.0 = aucun filtre
     * @param gainHF Gain hautes fréquences (0.0 → 1.0), 1.0 = aucun filtre
     *
     * Nécessite l'extension OpenAL EFX (AL_EXT_EFX).
     * Si l'extension est absente, la méthode ne fait rien.
     */
    void setLowPassFilter(float gainLF, float gainHF);

    /**
     * @brief Soumet cette source à un effet de réverbération (écho/réverb)
     * @param effectSlot ID OpenAL de l'auxiliary effect slot créé par SoundManager
     *
     * Si effectSlot == 0 ou que EFX n'est pas disponible, détache l'effet.
     */
    void setReverbEffect(unsigned int effectSlot);

    // --- Getters ---

    glm::vec3 getPosition() const { return m_position; }
    float     getGain()     const { return m_gain; }
    float     getPitch()    const { return m_pitch; }
    bool      isLooping()   const { return m_loop; }
    bool      isPlaying()   const;
    bool      isPaused()    const;

    const std::string& getFilePath() const { return m_filePath; }

private:
    std::string   m_filePath;
    glm::vec3     m_position;
    glm::vec3     m_velocity = glm::vec3(0.0f);

    float m_gain;
    float m_pitch;
    bool  m_loop;

    unsigned int m_buffer = 0;   // OpenAL buffer (données PCM)
    unsigned int m_source = 0;   // OpenAL source  (émetteur)
    unsigned int m_filter = 0;   // OpenAL EFX filter (passe-bas, optionnel)

    // Distance model
    float m_referenceDistance = 1.0f;
    float m_maxDistance = 100.0f;
    float m_rolloffFactor = 1.0f;

    /**
     * @brief Charge un fichier .wav et remplit m_buffer
     * @throws std::runtime_error si le fichier est introuvable ou invalide
     */
    void loadWav(const std::string& filePath);

    /**
     * @brief Applique tous les paramètres courants à la source OpenAL
     *
     * Appelée une seule fois à la fin du constructeur ; les setters
     * individuels mettent à jour en temps réel.
     */
    void applyAll();
};