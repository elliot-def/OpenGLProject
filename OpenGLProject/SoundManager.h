#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <glm/glm.hpp>
#include <AL/alc.h>

class Sound;

struct GLFWwindow;
struct ALCdevice_struct;
struct ALCcontext_struct;

/**
 * @enum ReverbPreset
 * @brief Préréglages de réverbération prêts à l'emploi
 *
 * Inspiré des presets EFX standard (EFXEAXREVERBPROPERTIES).
 */
enum class ReverbPreset {
    NONE,        ///< Pas de réverbération
    ROOM,        ///< Petite salle intérieure
    HALLWAY,     ///< Couloir étroit
    CAVE,        ///< Grotte / caverne
    ARENA,       ///< Grande salle
    OUTDOORS,    ///< Extérieur ouvert (quasi pas d'écho)
    UNDERWATER   ///< Sous l'eau (filtre + pitch modifié)
};

/**
 * @class SoundManager
 * @brief Gère l'ensemble du système audio 3D
 *
 * Responsabilités :
 *  - Initialisation / destruction du device et du contexte OpenAL
 *  - Chargement et mise en cache des sons (un son = un fichier)
 *  - Mise à jour de l'écouteur (listener) selon la caméra/joueur
 *  - Création et gestion des auxiliary effect slots (EFX reverb)
 *  - Contrôle global (volume maître, pause/resume, mute)
 *
 * Usage typique :
 * @code
 * SoundManager audio;
 * Sound* ambiance = audio.load("ambient", "res/sounds/wind.wav", true);
 * ambiance->play();
 *
 * Sound* step = audio.load("footstep", "res/sounds/step.wav");
 * step->setPosition(playerPos);
 * step->play();
 *
 * // Chaque frame :
 * audio.setListenerTransform(cameraPos, cameraFront, cameraUp);
 * audio.update();  // nettoyage des sons terminés, etc.
 * @endcode
 */
class SoundManager {
public:
    SoundManager();
    ~SoundManager();

    // ─── Chargement ──────────────────────────────────────────────────────────

    /**
     * @brief Charge un son et l'enregistre sous une clé nommée
     *
     * Si un son portant ce nom existe déjà, il est retourné directement
     * (pas de double chargement).
     *
     * @param name     Identifiant unique (ex: "footstep", "ambient")
     * @param filePath Chemin vers le fichier .wav
     * @param loop     Lecture en boucle ?
     * @param gain     Volume initial (0.0 → ∞)
     * @param pitch    Pitch initial (1.0 = normal)
     * @return Pointeur vers le Sound créé (géré par SoundManager)
     */
    Sound* load(const std::string& name,
        const std::string& filePath,
        bool  loop = false,
        float gain = 1.0f,
        float pitch = 1.0f);

    /**
     * @brief Supprime un son nommé et libère ses ressources OpenAL
     */
    void unload(const std::string& name);

    /**
     * @brief Supprime tous les sons chargés
     */
    void unloadAll();

    // ─── Accès ───────────────────────────────────────────────────────────────

    /**
     * @brief Récupère un son par son nom
     * @throws std::out_of_range si le nom est inconnu
     */
    Sound* get(const std::string& name);

    // ─── Contrôle global ─────────────────────────────────────────────────────

    /**
     * @brief Volume maître appliqué au listener (0.0 = silence, 1.0 = normal)
     *
     * Agit sur le gain du listener OpenAL, ce qui affecte toutes les sources.
     */
    void setMasterVolume(float volume);
    float getMasterVolume() const { return m_masterVolume; }

    /**
     * @brief Met en pause / reprend tous les sons actifs
     */
    void pauseAll();
    void resumeAll();

    /**
     * @brief Arrête tous les sons
     */
    void stopAll();

    /**
     * @brief Active/désactive le son (mute = gain listener à 0, sans oublier la valeur)
     */
    void setMute(bool mute);
    bool isMuted() const { return m_isMuted; }
    void toggleMute();

    void window_focus_callback(GLFWwindow* window, int focused);

    // ─── Listener (caméra / joueur) ───────────────────────────────────────────

    /**
     * @brief Met à jour la position et l'orientation de l'écouteur
     *
     * Doit être appelé chaque frame avec les données caméra.
     *
     * @param position Position mondiale de l'écouteur
     * @param front    Vecteur forward (direction du regard)
     * @param up       Vecteur up (haut de la tête)
     * @param velocity Vitesse de l'écouteur (pour l'effet Doppler, optionnel)
     */
    void setListenerTransform(glm::vec3 position,
        glm::vec3 front,
        glm::vec3 up,
        glm::vec3 velocity = glm::vec3(0.0f));

    // ─── Effet Doppler ────────────────────────────────────────────────────────

    /**
     * @brief Facteur Doppler global (0 = désactivé, 1 = physique, >1 = exagéré)
     */
    void setDopplerFactor(float factor);

    /**
     * @brief Vitesse du son utilisée pour le calcul Doppler (343 m/s par défaut)
     */
    void setSpeedOfSound(float speed);

    // ─── Réverbération (EFX) ─────────────────────────────────────────────────

    /**
     * @brief Initialise un effet de réverbération sur la scène
     *
     * Crée un auxiliary effect slot avec les paramètres du preset.
     * Les sons peuvent ensuite s'y connecter via Sound::setReverbEffect().
     *
     * @param preset Préréglage à appliquer
     * @return ID OpenAL de l'effect slot (0 si EFX non disponible)
     */
    unsigned int createReverbEffect(ReverbPreset preset = ReverbPreset::ROOM);

    /**
     * @brief Supprime un effect slot créé précédemment
     */
    void destroyReverbEffect(unsigned int effectSlot);

    /**
     * @brief Applique un preset de réverb à tous les sons chargés
     *
     * Pratique pour changer d'ambiance sonore globalement
     * (ex: entrer dans une grotte).
     *
     * @param preset ReverbPreset à appliquer, NONE pour détacher
     */
    void applyReverbToAll(ReverbPreset preset);

    // ─── Mise à jour ──────────────────────────────────────────────────────────

    /**
     * @brief À appeler chaque frame
     *
     * - Nettoie les sons non-loopés qui ont terminé leur lecture
     *   si cleanupFinished == true dans le constructeur
     */
    void update();

    // ─── Utilitaires ──────────────────────────────────────────────────────────

    /**
     * @brief Affiche dans la console tous les sons chargés et leur état
     */
    void printSoundList() const;

    /**
     * @brief Vérifie si l'extension EFX (effets avancés) est disponible
     */
    bool isEFXAvailable() const { return m_efxAvailable; }

private:
    ALCdevice* m_device = nullptr;
    ALCcontext* m_context = nullptr;

    std::unordered_map<std::string, std::unique_ptr<Sound>> m_sounds;
    std::vector<unsigned int> m_effectSlots;   // Auxiliary effect slots EFX

    float m_masterVolume = 1.0f;
    bool  m_isMuted = false;
    bool  m_efxAvailable = false;

    // Dernier effect slot actif (pour applyReverbToAll)
    unsigned int m_currentReverbSlot = 0;

    /**
     * @brief Initialise le device et le contexte OpenAL
     * @throws std::runtime_error si l'initialisation échoue
     */
    void initOpenAL();

    /**
     * @brief Libère le device et le contexte OpenAL
     */
    void shutdownOpenAL();

    /**
     * @brief Remplit les paramètres EAX/EFX selon le preset
     */
    void applyReverbPreset(unsigned int effect, ReverbPreset preset);
};