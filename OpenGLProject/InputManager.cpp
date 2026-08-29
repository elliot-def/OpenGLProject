#include "Log.h"
#include "InputManager.h"
#include "MenuManager.h"
#include "Game.h"
#include "Entity.h"
#include "PlayerKey.h"
#include "Escape.h"
#include "Mouse.h"
#include "Push.h"
#include "Grab.h"
#include "Window.h"
#include "FirstPersonArms.h"
#include "Controller.h"
#include "GlfwController.h"
#include "SteamController.h"
#include "ControllerKey.h"
#include "InputNotification.h"
#include "File.h"
#include "SteamManager.h"

#include "constants/file.h"

#include <nlohmann/json.hpp>
#include <cmath>

using json = nlohmann::json;

// ── CORRECTION TEMPORAIRE : Steam Input force hors-ligne ─────────────────
// Avec l'AppID placeholder 480 (Spacewar), SteamAPI_InitEx suffit a faire
// cacher la manette du XInput par le client Steam, mais la config Steam Input
// du jeu (manifest 'Game', bloc "configurations" vide) n'est jamais chargee :
// la manette tombe dans un trou (voir SteamController::init()). On force donc
// XInput/GLFW meme en ligne. Pour reactiver Steam Input, repasser ce flag a
// true ET fournir un vrai AppID Steamworks + une configuration dans le manifest.
constexpr bool kUseSteamInput = false;

InputManager::InputManager(Game* game, MenuManager* menuManager, Window* window, Player* player)
    : m_game(game), m_menuManager(menuManager), m_window(window), m_player(player) {
    m_notification = std::make_unique<InputNotification>();

    // ── Choix du mode manette ──
    // En ligne (Steam initialise), le client Steam masque la manette du
    // XInput : on lit via l'API Steam Input. Hors-connexion, ou si Steam
    // Input n'est pas disponible, on garde XInput (GLFW). Le repli dynamique
    // (Steam Input sans manette -> XInput) est gere dans update().
    m_glfwController = std::make_unique<GlfwController>();
    m_controller = m_glfwController.get();

    bool steamOnline = m_game && m_game->getSteamManager()
                    && m_game->getSteamManager()->isInitialized();
    if (kUseSteamInput && steamOnline) {
        m_steamController = std::make_unique<SteamInputController>();
        if (m_steamController->isAvailable()) {
            m_controller = m_steamController.get();
            logOut() << "[Input] Mode manette : Steam Input (Steam connecte).\n";
        } else {
            logOut() << "[Input] Steam Input indisponible, repli sur XInput (GLFW).\n";
            m_steamController.reset();
        }
    } else {
        logOut() << "[Input] Mode manette : XInput (GLFW).\n";
    }

    // Bindings depuis res/keys.json (fallback sur les defauts de ConfigKeys)
    // AVANT loadKeys() qui utilise m_keyBindings pour creer les Key.
    loadKeyBindings();
    loadControllerBindings();
    loadKeys();
    // Applique la sensibilite souris chargee depuis res/keys.json (le Mouse
    // vient d'etre cree dans loadKeys()).
    if (m_mouse) m_mouse->setSensitivity(m_mouseSensitivity);
    loadControllerKeys();
}

InputManager::~InputManager() = default;

Key* InputManager::getKey(const std::string& name) {
	auto it = m_keys.find(name);
	if (it != m_keys.end()) {
		return it->second.get();
	}
	throw std::out_of_range("Key not found: " + name);
}

void InputManager::setFirstPersonArms(FirstPersonArms* arms) {
	m_firstPersonArms = arms; // utilise par les boutons Push/Grab de la manette
	if (m_mouse) {
		m_mouse->setFirstPersonArms(arms);
	}
	auto pushIt = m_keys.find("Push");
	if (pushIt != m_keys.end()) {
		static_cast<Push*>(pushIt->second.get())->setFirstPersonArms(arms);
	}
	auto grabIt = m_keys.find("Grab");
	if (grabIt != m_keys.end()) {
		static_cast<Grab*>(grabIt->second.get())->setFirstPersonArms(arms);
	}
}

void InputManager::loadKeys() {
	m_keys["Forward"]   = std::make_unique<PlayerKey>(m_player, "Forward",   m_keyBindings["Forward"],  nullptr, nullptr, [this]() { m_player->processDirectionKey(EntityRelativeDirection::FORWARD); });
	m_keys["Backward"]  = std::make_unique<PlayerKey>(m_player, "Backward",  m_keyBindings["Backward"], nullptr, nullptr, [this]() { m_player->processDirectionKey(EntityRelativeDirection::BACKWARD); });
	m_keys["Left"]      = std::make_unique<PlayerKey>(m_player, "Left",      m_keyBindings["Left"],     nullptr, nullptr, [this]() { m_player->processDirectionKey(EntityRelativeDirection::LEFT); });
	m_keys["Right"]     = std::make_unique<PlayerKey>(m_player, "Right",     m_keyBindings["Right"],    nullptr, nullptr, [this]() { m_player->processDirectionKey(EntityRelativeDirection::RIGHT); });
	m_keys["Crouch"]    = std::make_unique<PlayerKey>(m_player, "Crouch",    m_keyBindings["Crouch"],   nullptr, nullptr, [this]() { m_player->processDirectionKey(EntityRelativeDirection::DOWN); });

	m_keys["Jump"]      = std::make_unique<PlayerKey>(m_player, "Jump",      m_keyBindings["Jump"],
	    [this]() { m_player->processJump(); },                                         // onPress : saut normal
	    nullptr,
	    [this]() { m_player->processDirectionKey(EntityRelativeDirection::UP); });     // ifPressed : vol noclip
	m_keys["Sprint"]    = std::make_unique<PlayerKey>(m_player, "Sprint",    m_keyBindings["Sprint"],   [this]() { m_player->setIsSprinting(true); },
	                                                                                                [this]() { m_player->setIsSprinting(false); });
	m_keys["Flashlight"]  = std::make_unique<PlayerKey>(m_player, "Flashlight",  m_keyBindings["Flashlight"],  nullptr, [this]() { m_player->processFlashLightKey(); });
	m_keys["ThirdPerson"] = std::make_unique<PlayerKey>(m_player, "ThirdPerson", m_keyBindings["ThirdPerson"], nullptr, [this]() { m_player->processThirdPersonKey(); });

	m_keys["Escape"] = std::make_unique<Escape>(m_game);
	// Echap n'est pas reconfigurable dans le menu (il permet de sortir des
	// menus), mais le binding stocke dans keys.json reste applique
	m_keys["Escape"]->setKey(m_keyBindings["Escape"]);
	m_keys["Push"]   = std::make_unique<Push>();
	m_keys["Push"]->setKey(m_keyBindings["Push"]);
	m_keys["Grab"]   = std::make_unique<Grab>();
	m_keys["Grab"]->setKey(m_keyBindings["Grab"]);

	// Touche de debug : HUD des animations du personnage 3P (off par defaut)
	auto debugHudKey = std::make_unique<Key>(nullptr, "DebugHUD", m_keyBindings["DebugHUD"]);
	debugHudKey->setOnPressAction(InputContext::GAME, [this]() { m_game->toggleDebugHUD(); });
	m_keys["DebugHUD"] = std::move(debugHudKey);

	m_keys["Noclip"] = std::make_unique<PlayerKey>(m_player, "Noclip", m_keyBindings["Noclip"],
	    [this]() { m_player->setUseGravity(!m_player->isGravityEnabled()); });
	
	m_mouse = std::make_unique<Mouse>(m_player, m_menuManager);
}

// ── Bindings persistes (res/keys.json) ─────────────────────────────────────

void InputManager::loadKeyBindings() {
	// Valeurs par defaut (ConfigKeys) : les reglages du JSON viennent par-dessus
	m_keyBindings = {
	    { "Forward",     ConfigKeys::KEY_FORWARD },
	    { "Backward",    ConfigKeys::KEY_BACKWARD },
	    { "Left",        ConfigKeys::KEY_LEFT },
	    { "Right",       ConfigKeys::KEY_RIGHT },
	    { "Crouch",      ConfigKeys::KEY_CROUCH },
	    { "Jump",        ConfigKeys::KEY_JUMP },
	    { "Sprint",      ConfigKeys::KEY_SPRINT },
	    { "Flashlight",  ConfigKeys::KEY_FLASHLIGHT },
	    { "ThirdPerson", ConfigKeys::KEY_THIRD_PERSON },
	    { "Escape",      ConfigKeys::KEY_ESCAPE },
	    { "Push",        ConfigKeys::KEY_PUSH },
	    { "Grab",        ConfigKeys::KEY_GRAB },
	    { "Noclip",      ConfigKeys::KEY_NOCLIP },
	    { "DebugHUD",    ConfigKeys::KEY_DEBUG_HUD },
	};

	// Sensibilites par defaut (ecrasees par keys.json si present)
	m_mouseSensitivity = ConfigKeys::DEFAULT_MOUSE_SENSITIVITY;
	m_controllerLookSensitivity = ConfigKeys::CONTROLLER_LOOK_SENSITIVITY;

	File keysFile(Constants::File::JSON_KEYS_PATH);
	if (!keysFile.exists()) {
	    logOut() << "[Input] Aucun fichier " << Constants::File::JSON_KEYS_PATH
	              << ", utilisation des touches par defaut.\n";
	    return;
	}

	try {
	    json j = json::parse(keysFile.readAll());

	    if (j.contains("bindings") && j["bindings"].is_object()) {
	        for (auto& [action, binding] : j["bindings"].items()) {
	            // On ne retient que les actions connues, avec un code de touche valide
	            if (m_keyBindings.count(action) == 0) continue;
	            if (!binding.is_number_integer()) continue;
	            int code = binding.get<int>();
	            if (code < 0 || code > GLFW_KEY_LAST) continue;
	            m_keyBindings[action] = code;
	        }
	    }

	    // Sensibilites stockees avec les bindings
	    if (j.contains("mouse_sensitivity") && j["mouse_sensitivity"].is_number()) {
	        m_mouseSensitivity = j["mouse_sensitivity"].get<float>();
	    }
	    if (j.contains("controller_sensitivity") && j["controller_sensitivity"].is_number()) {
	        m_controllerLookSensitivity = j["controller_sensitivity"].get<float>();
	    }

	    logOut() << "[Input] Touches chargees depuis " << Constants::File::JSON_KEYS_PATH << ".\n";
	}
	catch (const json::parse_error& e) {
	    logErr() << "[Input] Erreur dans " << Constants::File::JSON_KEYS_PATH
	              << " : " << e.what() << " (touches par defaut conservees)\n";
	}
}

void InputManager::loadControllerBindings() {
	// Valeurs par defaut (ConfigKeys) : les reglages du JSON viennent par-dessus
	m_controllerBindings = {
	    { "Jump",        ConfigKeys::CONTROLLER_JUMP },
	    { "Crouch",      ConfigKeys::CONTROLLER_CROUCH },
	    { "Push",        ConfigKeys::CONTROLLER_PUSH },
	    { "ThirdPerson", ConfigKeys::CONTROLLER_THIRD_PERSON },
	    { "Flashlight",  ConfigKeys::CONTROLLER_FLASHLIGHT },
	    { "Grab",        ConfigKeys::CONTROLLER_GRAB },
	    { "Noclip",      ConfigKeys::CONTROLLER_NOCLIP },
	    { "Escape",      ConfigKeys::CONTROLLER_ESCAPE },
	};

	File keysFile(Constants::File::JSON_KEYS_PATH);
	if (!keysFile.exists()) return;

	try {
	    json j = json::parse(keysFile.readAll());
	    if (!j.contains("controller_bindings") || !j["controller_bindings"].is_object()) return;

	    for (auto& [action, binding] : j["controller_bindings"].items()) {
	        if (m_controllerBindings.count(action) == 0) continue;
	        if (!binding.is_number_integer()) continue;
	        int code = binding.get<int>();
	        if (code < 0 || code > GLFW_GAMEPAD_BUTTON_LAST) continue;
	        m_controllerBindings[action] = code;
	    }
	    logOut() << "[Input] Boutons manette charges depuis " << Constants::File::JSON_KEYS_PATH << ".\n";
	}
	catch (const json::parse_error& e) {
	    logErr() << "[Input] Erreur dans " << Constants::File::JSON_KEYS_PATH
	              << " : " << e.what() << " (boutons manette par defaut conserves)\n";
	}
}

void InputManager::saveBindings() {
	json j;
	j["bindings"] = json::object();
	for (const auto& [action, code] : m_keyBindings) {
	    j["bindings"][action] = code;
	}
	j["controller_bindings"] = json::object();
	for (const auto& [action, button] : m_controllerBindings) {
	    j["controller_bindings"][action] = button;
	}

	// Sensibilites stockees avec les bindings. Arrondies a 2 decimales et
	// stockees en double : un float32 (ex. 0.30f) converti tel quel donnerait
	// "0.30000001192092896" dans le JSON.
	j["mouse_sensitivity"] = std::round(static_cast<double>(m_mouseSensitivity) * 100.0) / 100.0;
	j["controller_sensitivity"] = std::round(static_cast<double>(m_controllerLookSensitivity) * 100.0) / 100.0;

	File keysFile(Constants::File::JSON_KEYS_PATH);
	if (!keysFile.writeText(j.dump(2))) {
	    logErr() << "[Input] Impossible d'ecrire " << Constants::File::JSON_KEYS_PATH << ".\n";
	}
}

void InputManager::setKeyBinding(const std::string& action, int keyCode) {
	auto it = m_keys.find(action);
	if (it == m_keys.end()) {
	    logErr() << "[Input] Action inconnue pour le rebinding : " << action << "\n";
	    return;
	}
	// Met a jour la touche vivante + le binding persiste, puis sauvegarde
	it->second->setKey(keyCode);
	m_keyBindings[action] = keyCode;
	saveBindings();
}

void InputManager::setControllerBinding(const std::string& action, int button) {
	auto it = m_controllerKeys.find(action);
	if (it == m_controllerKeys.end()) {
	    logErr() << "[Input] Action manette inconnue pour le rebinding : " << action << "\n";
	    return;
	}
	it->second->setKey(button);
	m_controllerBindings[action] = button;
	saveBindings();
}

int InputManager::getControllerBinding(const std::string& action) const {
	auto it = m_controllerBindings.find(action);
	if (it != m_controllerBindings.end()) return it->second;
	return -1;
}

int InputManager::getKeyBinding(const std::string& action) const {
	auto it = m_keyBindings.find(action);
	if (it != m_keyBindings.end()) return it->second;
	return GLFW_KEY_UNKNOWN;
}

void InputManager::beginKeyCapture() {
	m_isCapturingKey = true;
	// Snapshot du clavier : une touche deja enfoncee au moment du clic ne
	// compte pas comme une nouvelle pression (edge detection pendant la capture)
	for (int key = 0; key <= GLFW_KEY_LAST; ++key) {
	    m_capturePrevState[key] = glfwGetKey(glfwGetCurrentContext(), key) == GLFW_PRESS;
	}
}

void InputManager::beginControllerCapture() {
	m_isCapturingControllerButton = true;
	// Pas de snapshot necessaire : isButtonJustPressed() gere deja l'edge
	// detection (un bouton deja enfonce ne sera pas capture).
}

void InputManager::setMouseSensitivity(float sensitivity) {
	m_mouseSensitivity = sensitivity;
	if (m_mouse) m_mouse->setSensitivity(sensitivity);
	saveBindings();
}

float InputManager::getMouseSensitivity() const {
	return m_mouseSensitivity;
}

void InputManager::setControllerLookSensitivity(float sensitivity) {
	m_controllerLookSensitivity = sensitivity;
	saveBindings();
}

void InputManager::loadControllerKeys() {
	Controller* controller = m_controller;

	// A : saut (meme logique que la touche Jump du clavier)
	m_controllerKeys["Jump"] = std::make_unique<ControllerKey>(m_player, "Jump", m_controllerBindings["Jump"], controller,
	    [this]() { m_player->processJump(); },                                        // onPress : saut normal
	    nullptr,
	    [this]() { m_player->processDirectionKey(EntityRelativeDirection::UP); });     // ifPressed : vol noclip

	// B : s'accroupir (tenu)
	m_controllerKeys["Crouch"] = std::make_unique<ControllerKey>(m_player, "Crouch", m_controllerBindings["Crouch"], controller,
	    nullptr, nullptr,
	    [this]() { m_player->processDirectionKey(EntityRelativeDirection::DOWN); });

	// X : pousser (comme R au clavier)
	m_controllerKeys["Push"] = std::make_unique<ControllerKey>(m_player, "Push", m_controllerBindings["Push"], controller,
	    [this]() {
	        if (m_firstPersonArms) m_firstPersonArms->triggerPush();
	    });

	// Y : premiere / troisieme personne (comme C au clavier, au relache)
	m_controllerKeys["ThirdPerson"] = std::make_unique<ControllerKey>(m_player, "ThirdPerson", m_controllerBindings["ThirdPerson"], controller,
	    nullptr, [this]() { m_player->processThirdPersonKey(); });

	// LB : torche (comme T au clavier, au relache)
	m_controllerKeys["Flashlight"] = std::make_unique<ControllerKey>(m_player, "Flashlight", m_controllerBindings["Flashlight"], controller,
	    nullptr, [this]() { m_player->processFlashLightKey(); });

	// RB : attraper (comme E au clavier)
	m_controllerKeys["Grab"] = std::make_unique<ControllerKey>(m_player, "Grab", m_controllerBindings["Grab"], controller,
	    [this]() {
	        if (m_firstPersonArms) m_firstPersonArms->triggerGrab();
	    });

	// Back : noclip (comme N au clavier)
	m_controllerKeys["Noclip"] = std::make_unique<ControllerKey>(m_player, "Noclip", m_controllerBindings["Noclip"], controller,
	    [this]() { m_player->setUseGravity(!m_player->isGravityEnabled()); });

	// Start : ouvrir le menu en jeu / reprendre dans les menus (comme Echap)
	// Actions reglees par contexte apres construction (GAME / MENU / PAUSED)
	auto escape = std::make_unique<ControllerKey>(m_player, "Escape", m_controllerBindings["Escape"], controller);
	escape->setOnReleaseAction(InputContext::GAME, [this]() {
	    m_game->changeState(GameState::STATE_MENU);
	});
	escape->setOnReleaseAction(InputContext::MENU, [this]() {
	    m_game->changeState(GameState::STATE_PLAYING);
	});
	escape->setOnReleaseAction(InputContext::PAUSED, [this]() {
	    m_game->changeState(GameState::STATE_PLAYING);
	});
	m_controllerKeys["Escape"] = std::move(escape);

	// B : retour au menu precedent (equivalent manette du bouton "Retour").
	// Fixe (non rebindable) : en jeu, B reste l'accroupissement via la touche
	// Crouch, seule l'action de menu est enregistree ici (contexte MENU couvre
	// STATE_MENU et STATE_OPTIONS).
	auto back = std::make_unique<ControllerKey>(m_player, "Back", GLFW_GAMEPAD_BUTTON_B, controller);
	back->setOnReleaseAction(InputContext::MENU, [this]() { m_menuManager->goBack(); });
	m_controllerKeys["Back"] = std::move(back);
}

void InputManager::updateController(InputContext context, int skipButton) {
	// ── Stick droit : regard (equivalent manette de la souris) ──
	// Le stick pilote la camera uniquement en jeu (comme la souris).
	if (context == InputContext::GAME) {
	    float lookX = m_controller->getAxisValue(ConfigKeys::CONTROLLER_LOOK_X_AXIS);
	    float lookY = m_controller->getAxisValue(ConfigKeys::CONTROLLER_LOOK_Y_AXIS);
	    if (lookX != 0.0f || lookY != 0.0f) {
	        // Stick vers le haut (valeur negative) => regarder vers le haut
	        m_player->processMouseMovements(
	            lookX * m_controllerLookSensitivity,
	            -lookY * m_controllerLookSensitivity);
	    }

	    // ── Stick gauche : deplacement analogique ──
	    float moveX = m_controller->getAxisValue(ConfigKeys::CONTROLLER_MOVE_X_AXIS);
	    float moveY = m_controller->getAxisValue(ConfigKeys::CONTROLLER_MOVE_Y_AXIS);
	    // Le facteur (0..1) module la vitesse : pleine vitesse en butee de stick
	    if (moveY < 0.0f) m_player->processDirectionKey(EntityRelativeDirection::FORWARD, -moveY);
	    if (moveY > 0.0f) m_player->processDirectionKey(EntityRelativeDirection::BACKWARD, moveY);
	    if (moveX < 0.0f) m_player->processDirectionKey(EntityRelativeDirection::LEFT, -moveX);
	    if (moveX > 0.0f) m_player->processDirectionKey(EntityRelativeDirection::RIGHT, moveX);

	    // ── Gachette droite (RT) : sprint (tenu) ──
	    // L'etat n'est touche qu'au changement (et non a chaque frame) pour
	    // ne pas ecraser la touche Shift du clavier quand la manette est
	    // connectee mais inutilisee.
	    bool wantSprint = m_controller->getAxisValue(ConfigKeys::CONTROLLER_SPRINT_AXIS) > ConfigKeys::CONTROLLER_TRIGGER_THRESHOLD;
	    if (wantSprint != m_controllerSprintActive) {
	        m_controllerSprintActive = wantSprint;
	        m_player->setIsSprinting(wantSprint);
	    }
	}

	// ── Boutons : actions (dispatch par contexte dans ControllerKey) ──
	// Le bouton fraichement capture pour un rebinding est ignore ce frame,
	// pour qu'il ne declenche pas aussi son action (ex: Start/Echap).
	for (const auto& pair : m_controllerKeys) {
	    if (skipButton >= 0 && pair.second->getKey() == skipButton) continue;
	    pair.second->update(context);
	}
}

// ── Navigation manette dans les menus (stick gauche + A) ─────────────────

void InputManager::updateMenuNavigation() {
	if (!m_controller || !m_controller->isConnected()) return;

	// A : valider l'élément sélectionné (équivalent manette du clic)
	if (m_controller->isButtonJustPressed(GLFW_GAMEPAD_BUTTON_A)) {
	    m_menuManager->activateControllerSelection();
	}

	float x = navAxisValue(ConfigKeys::CONTROLLER_MOVE_X_AXIS);
	float y = navAxisValue(ConfigKeys::CONTROLLER_MOVE_Y_AXIS);

	// Stick haut/bas : déplace la sélection (haut = valeur négative)
	int stepY = axisStep(m_navAxisY, y);
	if (stepY != 0) {
	    m_menuManager->navigateController(stepY);
	}

	// Stick gauche/droite : ajuste le slider / l'option de la liste selectionnee
	int stepX = axisStep(m_navAxisX, x);
	if (stepX != 0) {
	    m_menuManager->adjustControllerSelection(stepX);
	}
}

int InputManager::axisStep(AxisNav& nav, float value) {
	const float kThreshold = 0.5f;       // au-delà de la zone morte du stick
	const float kInitialDelay = 0.45f;   // délai avant répétition
	const float kRepeatInterval = 0.12f; // période de répétition en maintien

	int dir = (value > kThreshold) ? 1 : (value < -kThreshold ? -1 : 0);
	if (dir == 0) {
	    nav.holdDir = 0;
	    nav.repeating = false;
	    return 0;
	}

	auto now = std::chrono::steady_clock::now();
	if (nav.holdDir != dir) {
	    // Front montant : un pas immédiat
	    nav.holdDir = dir;
	    nav.holdStart = now;
	    nav.repeating = false;
	    return dir;
	}

	float held = std::chrono::duration<float>(now - nav.holdStart).count();
	if (!nav.repeating) {
	    if (held >= kInitialDelay) {
	        nav.repeating = true;
	        nav.lastRepeat = now;
	        return dir;
	    }
	}
	else if (std::chrono::duration<float>(now - nav.lastRepeat).count() >= kRepeatInterval) {
	    nav.lastRepeat = now;
	    return dir;
	}
	return 0;
}

float InputManager::navAxisValue(int axis) const {
	// Croix directionnelle (D-pad) prioritaire : +-1.0 numerique si pressee,
	// sinon la valeur analogique du stick gauche. Utilise les boutons
	// GLFW_GAMEPAD_BUTTON_DPAD_* (meme source XInput que le stick).
	int negButton, posButton;
	if (axis == GLFW_GAMEPAD_AXIS_LEFT_Y) {
	    negButton = GLFW_GAMEPAD_BUTTON_DPAD_UP;    // haut
	    posButton = GLFW_GAMEPAD_BUTTON_DPAD_DOWN;  // bas
	} else {
	    negButton = GLFW_GAMEPAD_BUTTON_DPAD_LEFT;  // gauche
	    posButton = GLFW_GAMEPAD_BUTTON_DPAD_RIGHT; // droite
	}
	if (!m_controller) return 0.0f;
	if (m_controller->isButtonPressed(negButton)) return -1.0f;
	if (m_controller->isButtonPressed(posButton)) return 1.0f;
	return m_controller->getAxisValue(axis);
}

void InputManager::resetNavState() {
    // On amorce la direction avec la valeur courante (stick ou croix
    // directionnelle) : une direction encore tenue pendant la transition
    // n'est pas traitee comme une nouvelle pression (pas de pas immediat),
    // seule la repetition au maintien repart.
    auto prime = [](AxisNav& nav, float value) {
        nav.holdDir = (value > 0.5f) ? 1 : (value < -0.5f ? -1 : 0);
        nav.holdStart = std::chrono::steady_clock::now();
        nav.repeating = false;
    };
    const float y = navAxisValue(ConfigKeys::CONTROLLER_MOVE_Y_AXIS);
    const float x = navAxisValue(ConfigKeys::CONTROLLER_MOVE_X_AXIS);
    prime(m_navAxisY, y);
    prime(m_navAxisX, x);
}

void InputManager::update() {
	bool actionPerformed = false;

	// ── 0. Capture de touche (rebinding dans le menu Options) ──
	// Quand une capture est en cours, la premiere NOUVELLE pression est
	// capturee et la touche est "consommee" ce frame : son action ne se
	// declenche pas (ex: Echap n'ouvre pas le menu pendant la capture).
	bool consumeCapturedKey = false;
	int capturedKey = -1;
	if (m_isCapturingKey) {
	    for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; ++key) {
	        bool pressed = glfwGetKey(glfwGetCurrentContext(), key) == GLFW_PRESS;
	        bool freshPress = pressed && !m_capturePrevState[key];
	        m_capturePrevState[key] = pressed;
	        if (freshPress) {
	            capturedKey = key;
	            m_isCapturingKey = false;
	            break;
	        }
	    }
	    if (capturedKey != -1) {
	        consumeCapturedKey = true;
	        if (m_onKeyCaptured) m_onKeyCaptured(capturedKey);
	    }
	}

	// ── 1. Sondage des deux sources manette (XInput et/ou Steam Input) ──
	bool steamActivity = false;
	if (m_steamController) {
	    steamActivity = m_steamController->poll();
	}
	bool glfwActivity = m_glfwController->poll();

	// Choix dynamique de la source active : Steam Input si en ligne ET avec
	// une manette, sinon XInput. Couvre aussi le cas "Steam Input desactive
	// pour Xbox" (le client ne masque rien, XInput retrouve la manette).
	Controller* active = (m_steamController && m_steamController->isAvailable()
	                      && m_steamController->isConnected())
	                     ? static_cast<Controller*>(m_steamController.get())
	                     : static_cast<Controller*>(m_glfwController.get());
	if (active != m_controller) {
	    m_controller = active;
	    for (auto& pair : m_controllerKeys) {
	        pair.second->setController(m_controller);
	    }
	    logOut() << "[Input] Source manette active : "
	              << (m_controller == m_steamController.get() ? "Steam Input" : "XInput (GLFW)")
	              << "\n";
	}

	bool controllerActivity = steamActivity || glfwActivity;

	// ── 1b. Capture de bouton manette (rebinding dans le menu Options) ──
	int capturedButton = -1;
	if (m_isCapturingControllerButton && m_controller && m_controller->isConnected()) {
	    for (int b = 0; b <= GLFW_GAMEPAD_BUTTON_LAST; ++b) {
	        if (m_controller->isButtonJustPressed(b)) {
	            capturedButton = b;
	            m_isCapturingControllerButton = false;
	            if (m_onControllerButtonCaptured) m_onControllerButtonCaptured(b);
	            break;
	        }
	    }
	}

	// ── 2. Clavier / souris ──
	double xpos, ypos;
	glfwGetCursorPos(m_window->getGLFWwindow(), &xpos, &ypos);

	// Mouvement de souris = activite clavier/souris
	bool keyboardActivity = (xpos != m_lastMouseX) || (ypos != m_lastMouseY);
	m_lastMouseX = xpos;
	m_lastMouseY = ypos;

	// Nouvelle pression du bouton gauche = activite clavier/souris
	bool leftPressed = glfwGetMouseButton(glfwGetCurrentContext(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
	if (leftPressed && !m_lastLeftButtonPressed) keyboardActivity = true;
	m_lastLeftButtonPressed = leftPressed;

	actionPerformed |= m_mouse->update(m_context, xpos, ypos);
	for (const auto& pair : m_keys) {
		Key* key = pair.second.get();
		// La touche capturee ce frame ne declenche pas son action
		if (consumeCapturedKey && key->getKey() == capturedKey) continue;
		int state = glfwGetKey(glfwGetCurrentContext(), key->getKey());
		bool wasPressed = key->getStatus();
		if (state == GLFW_PRESS) {
			key->ifPressed(m_context);
			actionPerformed = true;
		}
		if (state == GLFW_PRESS && !wasPressed) {
			key->onPress(m_context);
			keyboardActivity = true; // nouvelle pression clavier = basculer
		}
		else if (state == GLFW_RELEASE && wasPressed) {
			key->onRelease(m_context);
		}
	}

	// ── 3. Branchement/debranchement + bascule dynamique de la source ──
	// Clavier/souris ET manette restent fonctionnels en permanence : la
	// bascule ne change que la source "active" (affichee par la notification
	// et consultable via getActiveSource()). La manette passe devant le
	// clavier si les deux arrivent le meme frame.
	const bool controllerConnected = m_controller->isConnected();
	const bool justConnected = controllerConnected && !m_lastControllerConnected;
	const bool justDisconnected = !controllerConnected && m_lastControllerConnected;

	if (justConnected) {
		// Manette branchee a chaud : bascule immediate + notification dediee
		m_activeSource = InputSource::CONTROLLER;
		m_notification->showConnected();
	}
	else if (justDisconnected) {
		// Manette debranchee : notification + retour clavier/souris si c'etait
		// la source active.
		m_notification->showDisconnected();
		if (m_activeSource == InputSource::CONTROLLER) {
			m_activeSource = InputSource::KEYBOARD_MOUSE;
		}
	}
	else if (controllerActivity && m_activeSource != InputSource::CONTROLLER) {
		m_activeSource = InputSource::CONTROLLER;
		m_notification->show(InputSource::CONTROLLER);
	}
	else if (keyboardActivity && m_activeSource != InputSource::KEYBOARD_MOUSE) {
		m_activeSource = InputSource::KEYBOARD_MOUSE;
		m_notification->show(InputSource::KEYBOARD_MOUSE);
	}
	m_lastControllerConnected = controllerConnected;

	// ── 4. Gameplay manette : sticks (regard/deplacement), gachettes, boutons ──
	if (m_controller->isConnected()) {
		updateController(m_context, capturedButton);
	}

	// ── 5. Navigation manette dans les menus (stick gauche + A) ──
	// Hors jeu uniquement, et pas pendant une capture de rebinding (la touche
	// capturee ne doit pas declencher d'action de menu).
	if (m_context != InputContext::GAME
	    && !m_isCapturingKey
	    && !m_isCapturingControllerButton
	    && capturedButton == -1) {
	    updateMenuNavigation();
	}

	// L'activite manette compte aussi comme "derniere action" (anti-AFK en menu)
	if (actionPerformed || controllerActivity) {
		resetLastUpdateTime();
	}
}
