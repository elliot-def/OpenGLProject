#pragma once
#include "Menu.h"

#include <memory>
#include <vector>

class Game;
class VoiceChat;

// Sous-menu "Audio" des Options : reglages du chat vocal (activation, mode
// VAD/push-to-talk, sensibilite, volume des voix, micro, device de capture).
// Les valeurs sont lues/ecrites directement dans VoiceChat::settings() et
// persistees par OptionsMenu::exportJSON (res/options.json).
class AudioSettingsMenu : public Menu {
public:
    AudioSettingsMenu(Game* game, SoundManager* soundManager,
                      std::vector<std::unique_ptr<TextRenderer>>* textRenderers,
                      ShaderManager* shaderManager, CursorManager* cursorManager,
                      VoiceChat* voiceChat);

private:
    VoiceChat* m_voiceChat = nullptr;
};
