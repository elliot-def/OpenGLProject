#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// Registre central des chemins de ressources (modèles, sons, textures, polices).
//
// But : regrouper ici tous les chemins d'assets auparavant éparpillés en dur
// dans les .cpp/.h (ModelLoader, MainMenu, CursorManager, DVDShape, Window,
// Game...). Pour réorganiser un asset, on ne modifie qu'UNE seule ligne,
// au lieu de chercher la même chaîne dans tout le projet.
// ─────────────────────────────────────────────────────────────────────────────

namespace Constants {
namespace Resource {

    // ── Modèles 3D ────────────────────────────────────────────────────────
    inline constexpr const char* MODEL_BACKPACK     = "./res/models/backpack/backpack.obj";
    inline constexpr const char* MODEL_FROPY        = "./res/models/fropy/fropy_low_poly.obj";
    inline constexpr const char* MODEL_ARMS_RIG     = "./res/rigging/arm/arms_rig.glb";
    inline constexpr const char* MODEL_MEGAN        = "./res/rigging/mixamo/models/Megan.fbx";
    inline constexpr const char* MIXAMO_ANIM_DIR    = "./res/rigging/mixamo/animation/";

    // ── Sons / musiques ───────────────────────────────────────────────────
    inline constexpr const char* SOUND_MENU_MUSIC                  = "./res/sounds/menu/industry-garage-ventilation-system-01.wav";
    inline constexpr const char* SOUND_MENU_GHOST_BIRDS_03         = "./res/sounds/menu/atmo-horror-ghost-birds-03.wav";
    inline constexpr const char* SOUND_MENU_GHOST_BIRDS_02         = "./res/sounds/menu/atmo-horror-ghost-birds-02.wav";
    inline constexpr const char* SOUND_MENU_GHOST_BIRDS_01         = "./res/sounds/menu/atmo-horror-ghost-birds-01.wav";
    inline constexpr const char* SOUND_MENU_SWELLING_DUNGEON       = "./res/sounds/menu/musical-horror-swelling-dungeon-01.wav";
    inline constexpr const char* SOUND_MENU_SILENCE_INVESTIGATION  = "./res/sounds/menu/musical-horror-silence-investigation-01.wav";
    inline constexpr const char* SOUND_MENU_FISHMAN_GRUNT          = "./res/sounds/menu/creature-humanoid-fishman-grunt-02.wav";
    inline constexpr const char* SOUND_MENU_CLICK                  = "./res/sounds/menu/ui-click-generic-plastic-01.wav";
    inline constexpr const char* SOUND_MUSIC_ON_AND_ON             = "res/sounds/on&on.ogg";

    // ── Textures ──────────────────────────────────────────────────────────
    inline constexpr const char* TEXTURE_LOGO     = "./res/textures/logo.jpeg";
    inline constexpr const char* TEXTURE_DVD_LOGO = "./res/textures/menu/dvd_logo.png";

    // Curseurs (dossier "cursor assets" = nom original du projet)
    inline constexpr const char* TEXTURE_CURSOR_NONE       = "./res/textures/menu/cursor assets/cursor_none.png";
    inline constexpr const char* TEXTURE_CURSOR_BUSY       = "./res/textures/menu/cursor assets/busy_circle_fade.png";
    inline constexpr const char* TEXTURE_CURSOR_DISABLED   = "./res/textures/menu/cursor assets/disabled.png";
    inline constexpr const char* TEXTURE_CURSOR_CROSSHAIR  = "./res/textures/menu/cursor assets/target_a.png.png";
    inline constexpr const char* TEXTURE_CURSOR_TEXT       = "./res/textures/menu/cursor assets/bracket_a_vertical.png";
    inline constexpr const char* TEXTURE_CURSOR_GRAB       = "./res/textures/menu/cursor assets/hand_open.png";
    inline constexpr const char* TEXTURE_CURSOR_GRABBING   = "./res/textures/menu/cursor assets/hand_closed.png";

    // ── Polices ───────────────────────────────────────────────────────────
    inline constexpr const char* FONT_ARMANA           = "res/fonts/armana/Amarna-Bold.ttf";
    inline constexpr const char* FONT_GNOCCHI          = "res/fonts/Gnocchi.ttf";
    inline constexpr const char* FONT_KENNEY_STEAM     = "res/fonts/kenney/kenney_input_steam_controller.ttf";
    inline constexpr const char* FONT_KENNEY_KB_MOUSE  = "res/fonts/kenney/kenney_input_keyboard_&_mouse.ttf";

} // namespace Resource
} // namespace Constants
