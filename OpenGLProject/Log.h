#pragma once

#include <cstdio>

// ─────────────────────────────────────────────────────────────────────────────
// Logging unifie : remplace le melange printf/cout/cerr par des macros
// desactivables en release (#ifdef _DEBUG). Utilise printf-style pour eviter
// le cout des iostream en hot paths.
//
// Usage :
//   LOG_INFO("Modele charge : %s", path.c_str());
//   LOG_WARN("Bone %s non trouve", boneName.c_str());
//   LOG_ERROR("Echec chargement texture : %s", path.c_str());
// ─────────────────────────────────────────────────────────────────────────────

#ifdef _DEBUG
  #define LOG_INFO(fmt, ...)  std::printf("[INFO]  " fmt "\n", ##__VA_ARGS__)
  #define LOG_WARN(fmt, ...)  std::fprintf(stderr, "[WARN]  " fmt "\n", ##__VA_ARGS__)
  #define LOG_ERROR(fmt, ...) std::fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)
#else
  #define LOG_INFO(fmt, ...)  ((void)0)
  #define LOG_WARN(fmt, ...)  ((void)0)
  #define LOG_ERROR(fmt, ...) ((void)0)
#endif
