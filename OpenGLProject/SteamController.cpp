#include "SteamController.h"
#include "Log.h"

#include "dependencies/steam/steam_api.h"

#include <cmath>
#include <cstdio>
#include <chrono>
#include <filesystem>
#include <thread>

#include "constants/file.h"

// Initialise Steam Input : Init + manifest d'actions + resolution des
// handles d'actions (boutons et axes). Retourne false si Steam Input n'est
// pas disponible (ex: desactive pour les manettes Xbox), auquel cas
// InputManager se replie sur GlfwController (XInput).
bool SteamInputController::init() {
    ISteamInput* input = SteamInput();
    if (!input) {
        logPrintf("[SteamController] SteamInput() indisponible.\n");
        return false;
    }

    // bExplicitlyCallRunFrame = false : Steam Input est mis a jour par
    // SteamAPI_RunCallbacks() (deja appele chaque frame par SteamManager).
    if (!input->Init(false)) {
        logPrintf("[SteamController] SteamInput()->Init() a echoue (Steam Input desactive ?).\n");
        return false;
    }
    m_initialized = true;

    // Manifest d'actions : chemin ABSOLU requis par Steam.
    std::filesystem::path manifest =
        std::filesystem::absolute(Constants::File::STEAM_INPUT_MANIFEST_PATH);
    if (!input->SetInputActionManifestFilePath(manifest.string().c_str())) {
        logPrintf("[SteamController] Avertissement : manifest d'actions introuvable/invalide (%s).\n",
               manifest.string().c_str());
        // Non fatal : on continue, les handles d'actions seront peut-etre 0.
    }

    // Le manifest est applique de maniere asynchrone par Steam (IPC vers le
    // client Steam). On pompe SteamAPI_RunCallbacks() et on reessaie jusqu'a
    // ce que les action sets soient enregistres (timeout ~3s).
    m_actionSet = 0;
    for (int attempt = 0; attempt < 30 && !m_actionSet; ++attempt) {
        // RunFrame() ne vehicule que les etats boutons/axes basse latence.
        // L'enregistrement des action sets, lui, arrive via le flux de
        // callbacks Steam : il faut pomper SteamAPI_RunCallbacks().
        SteamAPI_RunCallbacks();
        m_actionSet = input->GetActionSetHandle("Game");
        if (!m_actionSet) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    if (!m_actionSet) {
        // Diagnostic : pourquoi aucun action set n'est visible ?
        //  - ship_controls/menu_controls : les action sets du vrai Spacewar (480)
        //    -> non nuls si Steam charge le manifest officiel de l'app au lieu du notre
        //  - configSettings : bitmask d'opt-in Steam Input de la session
        //    (0x0002 = Xbox, 0x0000 = aucun type opte -> Steam Input inactif)
        //  - nbControllers : manettes vues par Steam Input
        InputHandle_t tmpHandles[STEAM_INPUT_MAX_COUNT];
        int nbControllers = input->GetConnectedControllers(tmpHandles);
        uint16 cfg = input->GetSessionInputConfigurationSettings();
        logPrintf("[SteamController] Action set 'Game' introuvable. Diagnostic : "
               "ship_controls=%llu menu_controls=%llu configSettings=0x%04X nbControllers=%d\n",
               (unsigned long long)input->GetActionSetHandle("ship_controls"),
               (unsigned long long)input->GetActionSetHandle("menu_controls"),
               cfg, nbControllers);
        input->Shutdown();
        m_initialized = false;
        return false;
    }

    // Boutons : action digitale Steam -> index GLFW_GAMEPAD_BUTTON_*
    m_digital[GLFW_GAMEPAD_BUTTON_A]            = input->GetDigitalActionHandle("Jump");
    m_digital[GLFW_GAMEPAD_BUTTON_B]            = input->GetDigitalActionHandle("Crouch");
    m_digital[GLFW_GAMEPAD_BUTTON_X]            = input->GetDigitalActionHandle("Push");
    m_digital[GLFW_GAMEPAD_BUTTON_Y]            = input->GetDigitalActionHandle("ThirdPerson");
    m_digital[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER]  = input->GetDigitalActionHandle("Flashlight");
    m_digital[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] = input->GetDigitalActionHandle("Grab");
    m_digital[GLFW_GAMEPAD_BUTTON_BACK]         = input->GetDigitalActionHandle("Noclip");
    m_digital[GLFW_GAMEPAD_BUTTON_START]        = input->GetDigitalActionHandle("Escape");

    // Axes : une action analogique par groupe (deplacement / regard / sprint)
    m_move   = input->GetAnalogActionHandle("Move");
    m_look   = input->GetAnalogActionHandle("Look");
    m_sprint = input->GetAnalogActionHandle("Sprint");

    m_available = true;
    logPrintf("[SteamController] Steam Input initialise (action set 'Game').\n");
    return true;
}

SteamInputController::SteamInputController() {
    init();
}

SteamInputController::~SteamInputController() {
    if (m_initialized && SteamInput()) {
        SteamInput()->Shutdown();
    }
}

bool SteamInputController::poll() {
    if (!m_available) return false;

    ISteamInput* input = SteamInput();
    if (!input) return false;

    // Pas de RunFrame() ici : avec Init(false), Steam Input est mis a jour par
    // SteamAPI_RunCallbacks() (appele chaque frame par SteamManager::runCallbacks).

    // Enumere les manettes Steam Input actives pour ce jeu.
    InputHandle_t handles[STEAM_INPUT_MAX_COUNT];
    int count = input->GetConnectedControllers(handles);
    const bool wasConnected = m_connected;

    if (count <= 0) {
        if (m_connected && m_disconnectGraceFrames < kDisconnectGraceFrames) {
            // Delai de grace (meme logique que GlfwController) : filtre le
            // clignotement au branchement (Steam re-applique la config a la
            // manette fraichement branchee, la fait disparaitre 1-2 frames).
            ++m_disconnectGraceFrames;
            m_buttons = {};
            m_prevButtons = {};
            m_axes = {};
            m_prevAxes = {};
            return false;
        }
        if (m_connected) {
            logPrintf("[SteamController] Manette Steam Input deconnectee.\n");
        }
        m_connected = false;
        m_handle = 0;
        m_buttons = {};
        m_prevButtons = {};
        m_axes = {};
        m_prevAxes = {};
        return false;
    }

    m_handle = handles[0]; // premiere manette (suffisant en solo)
    m_disconnectGraceFrames = 0;
    m_connected = true;
    if (!wasConnected) {
        logPrintf("[SteamController] Manette Steam Input detectee (handle %llu).\n",
               static_cast<unsigned long long>(m_handle));
        // PAS de return ici : on lit l'etat et on (re)active l'action set des
        // ce frame, pour que la manette branchee a chaud soit fonctionnelle
        // immediatement (l'activation est asynchrone cote Steam).
    }

    // Etat precedent pour l'edge detection
    m_prevButtons = m_buttons;
    m_prevAxes = m_axes;

    // Active l'action set (re-joue chaque frame : c'est pas cher et ca tolere
    // les changements d'etat du jeu sans gestion fine des transitions).
    input->ActivateActionSet(m_handle, m_actionSet);

    // Boutons. bActive=false tant que la config Steam Input n'est pas
    // appliquee au controleur branche a chaud : on ignore ces donnees pour ne
    // pas traiter un etat transitoire comme une entree (ou un appui fantome).
    for (auto& entry : m_digital) {
        int button = entry.first;
        InputDigitalActionHandle_t handle = entry.second;
        if (handle) {
            InputDigitalActionData_t d = input->GetDigitalActionData(m_handle, handle);
            m_buttons[button] = d.bActive && d.bState;
        }
    }

    // Axes
    if (m_move) {
        InputAnalogActionData_t d = input->GetAnalogActionData(m_handle, m_move);
        m_axes[GLFW_GAMEPAD_AXIS_LEFT_X] = d.bActive ? d.x : 0.0f;
        m_axes[GLFW_GAMEPAD_AXIS_LEFT_Y] = d.bActive ? d.y : 0.0f;
    }
    if (m_look) {
        InputAnalogActionData_t d = input->GetAnalogActionData(m_handle, m_look);
        m_axes[GLFW_GAMEPAD_AXIS_RIGHT_X] = d.bActive ? d.x : 0.0f;
        m_axes[GLFW_GAMEPAD_AXIS_RIGHT_Y] = d.bActive ? d.y : 0.0f;
    }
    if (m_sprint) {
        InputAnalogActionData_t d = input->GetAnalogActionData(m_handle, m_sprint);
        m_axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] = d.bActive ? d.x : 0.0f;
    }

    // Activite = branchement a chaud OU un bouton vient d'etre presse OU un
    // axe vient de bouger (retourne true sur la connexion pour declencher la
    // bascule de source + la notification sans attendre un appui).
    if (m_connected && !wasConnected) return true;
    for (int b = 0; b <= GLFW_GAMEPAD_BUTTON_LAST; ++b) {
        if (isButtonJustPressed(b)) return true;
    }
    for (int a = 0; a <= GLFW_GAMEPAD_AXIS_LAST; ++a) {
        if (std::fabs(m_axes[a] - m_prevAxes[a]) > 0.05f) return true;
    }
    return false;
}

bool SteamInputController::isButtonPressed(int button) const {
    if (button < 0 || button > GLFW_GAMEPAD_BUTTON_LAST) return false;
    return m_connected && m_buttons[button];
}

bool SteamInputController::isButtonJustPressed(int button) const {
    if (button < 0 || button > GLFW_GAMEPAD_BUTTON_LAST) return false;
    return m_connected && m_buttons[button] && !m_prevButtons[button];
}

bool SteamInputController::isButtonJustReleased(int button) const {
    if (button < 0 || button > GLFW_GAMEPAD_BUTTON_LAST) return false;
    return m_connected && !m_buttons[button] && m_prevButtons[button];
}

float SteamInputController::getAxisValue(int axis) const {
    if (!m_connected || axis < 0 || axis > GLFW_GAMEPAD_AXIS_LAST) return 0.0f;
    float value = m_axes[axis];
    if (std::fabs(value) < STICK_DEADZONE) return 0.0f;
    return value;
}
