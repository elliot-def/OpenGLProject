#pragma once

// Prelude Windows obligatoire pour les TU qui touchent au rendu.
// Sans WIN32_LEAN_AND_MEAN, Windows SDK pull winsock.h / rpc.h / etc. - collision
// avec glad.h (APIENTRY redefini) + compile-time bloat.
//
// Ordre critique : glad.h definit APIENTRY (ligne 32) sous #ifndef, mais
// minwindef.h:130 (Win SDK 10.0.26100) le REDEFINIT sans garde -> C4005 si
// glad.h est traite en premier. win_compat.h force <windows.h> en PREMIERE
// inclusion -> le guard de glad.h kick in et le warning disparait.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
