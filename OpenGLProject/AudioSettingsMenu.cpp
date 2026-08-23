#include "AudioSettingsMenu.h"
#include "Game.h"
#include "MenuManager.h"
#include "VoiceChat.h"

#include "constants/window.h"

#include <algorithm>

AudioSettingsMenu::AudioSettingsMenu(Game* game, SoundManager* soundManager,
                                     std::vector<std::unique_ptr<TextRenderer>>* textRenderers,
                                     ShaderManager* shaderManager,
                                     CursorManager* cursorManager,
                                     VoiceChat* voiceChat)
    : Menu(game, soundManager, textRenderers, shaderManager, cursorManager,
           "Audio", false),
      m_voiceChat(voiceChat) {
    const VoiceChat::Settings& s = m_voiceChat ? m_voiceChat->settings()
                                               : VoiceChat::Settings{};

    // Chat vocal active / desactive
    addCheckbox("Chat vocal", Constants::Window::WINDOW_WIDTH / 2, 400, 40,
                s.enabled, [this](bool checked) {
                    if (m_voiceChat) m_voiceChat->settings().enabled = checked;
                });

    // Activation vocale (VAD) ou push-to-talk (touche V)
    addCheckbox("Activation vocale (VAD)", Constants::Window::WINDOW_WIDTH / 2,
                460, 40, s.voiceActivation, [this](bool checked) {
                    if (m_voiceChat) m_voiceChat->settings().voiceActivation = checked;
                });

    // Seuil de la detection vocale
    addRange("Sensibilite vocale", Constants::Window::WINDOW_WIDTH / 2, 520,
             300, 25, 0.0f, 1.0f, s.sensitivity, [this](float value) {
                 if (m_voiceChat) m_voiceChat->settings().sensitivity = value;
             });

    // Volume des voix des autres joueurs
    addRange("Volume des voix", Constants::Window::WINDOW_WIDTH / 2, 580,
             300, 25, 0.0f, 1.0f, s.volume, [this](float value) {
                 if (m_voiceChat) m_voiceChat->settings().volume = value;
             });

    // Device de capture (liste des micros + defaut systeme)
    {
        // Noms bruts OpenAL (stockes + utilises pour ouvrir le device).
        const std::vector<std::string> devices = VoiceChat::getCaptureDevices();
        // Libelles affiches dans le select : prefixe "OpenAL Soft on " retire.
        std::vector<std::string> labels;
        labels.reserve(devices.size());
        for (const std::string& d : devices) {
            labels.push_back(VoiceChat::cleanDeviceName(d));
        }

        int defaultIndex = 0;
        for (size_t i = 1; i < devices.size(); ++i) {
            if (devices[i] == s.device) { defaultIndex = static_cast<int>(i); break; }
        }
        addSelect("Micro", Constants::Window::WINDOW_WIDTH / 2, 640, 420, 30,
                  labels, defaultIndex, [this, devices](int index) {
                      if (!m_voiceChat) return;
                      m_voiceChat->settings().device =
                          (index <= 0) ? std::string() : devices[static_cast<size_t>(index)];
                  });
    }

    // Micro coupe (on n'envoie plus rien, on continue d'ecouter)
    addCheckbox("Micro coupe", Constants::Window::WINDOW_WIDTH / 2, 700, 40,
                s.micMuted, [this](bool checked) {
                    if (m_voiceChat) m_voiceChat->settings().micMuted = checked;
                });

    // Rappel de la touche push-to-talk (texte statique, non actionnable)
    addItem("Touche parler (mode PTT) : " + std::string(VoiceChat::PTT_KEY_NAME),
            Constants::Window::WINDOW_WIDTH / 2, 760, 600, 40, nullptr);

    addItem("Retour", Constants::Window::WINDOW_WIDTH / 2, 830, 200, 50,
            [this]() {
                m_game->getMenuManager()->showOptions();
            });
}
