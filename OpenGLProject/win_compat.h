#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// win_compat.h — Pont de compatibilité Windows ↔ macOS / Linux
//
// Sur Windows : force WIN32_LEAN_AND_MEAN avant <windows.h> pour éviter la
// collision APIENTRY avec glad.h (minwindef.h redéfinit APIENTRY sans garde).
//
// Sur macOS/Linux : ne fait rien. Les APIs Win32 (ShellExecute, Sleep, etc.)
// sont déjà protégées par des #ifdef _WIN32 dans les fichiers qui les utilisent.
// ─────────────────────────────────────────────────────────────────────────────

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif
