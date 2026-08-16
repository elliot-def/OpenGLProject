#pragma once
#include "Menu.h"
#include "gamestate.h"

class Game;
class ShaderManager;
class TextRenderer;
class VoiceChat;

class OptionsMenu : public Menu {
public:
    OptionsMenu(Game* game, SoundManager* soundManager, GameState& previousState, std::vector<std::unique_ptr<TextRenderer>>* textRenderers, ShaderManager* shaderManager, CursorManager* cursorManager, VoiceChat* voiceChat);
    ~OptionsMenu();

    // Charge/sauvegarde les options audio (res/options.json). Les bindings et
    // les sensibilites sont eux geres par l'InputManager (res/keys.json).
    void loadJSON();
    void exportJSON();

    void createOptions(bool isMuted, float volume, float musicVolume);
protected:
	GameState& m_previousState;
    VoiceChat* m_voiceChat = nullptr;
};
