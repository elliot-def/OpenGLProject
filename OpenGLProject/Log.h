#pragma once

#include <cstdio>
#include <cstdarg>
#include <chrono>
#include <ctime>
#include <iostream>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// Logging unifie : remplace le melange printf/cout/cerr par des macros
// desactivables en release (#ifdef _DEBUG). Utilise printf-style pour eviter
// le cout des iostream en hot paths.
//
// Chaque ligne est prefixee d'un horodatage local [HH:MM:SS.mmm].
//
// Usage :
//   LOG_INFO("Modele charge : %s", path.c_str());
//   LOG_WARN("Bone %s non trouve", boneName.c_str());
//   LOG_ERROR("Echec chargement texture : %s", path.c_str());
//
// Les printf() du projet sont remplaces par logPrintf() (meme signature),
// qui prefixe aussi l'horodatage :
//   logPrintf("[Game] Mode hors-ligne force.\n");
// ─────────────────────────────────────────────────────────────────────────────

// Affiche l'horodatage local [HH:MM:SS.mmm] sur le flux donne (stdout par
// defaut), sans retour a la ligne (le message suit sur la meme ligne).
inline void logTimestamp(std::FILE* stream = stdout) {
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto ms  = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    const std::time_t t = system_clock::to_time_t(now);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif
    std::fprintf(stream, "[%02d:%02d:%02d.%03d] ",
                 local.tm_hour, local.tm_min, local.tm_sec,
                 static_cast<int>(ms.count()));
}

// printf() horodate : meme signature que printf(), a utiliser a la place de
// printf dans tout le projet pour prefixer chaque ligne d'un horodatage.
inline void logPrintf(const char* fmt, ...) {
    logTimestamp(stdout);
    std::va_list args;
    va_start(args, fmt);
    std::vfprintf(stdout, fmt, args);
    va_end(args);
}

// LOG_* restent actifs en Release : utiles pour diagnostiquer chez les
// testeurs (le build Release est celui partage). Le surcout est negligeable
// car les appels LOG_* sont concentres dans le chargement/les evenements,
// pas dans les hot paths par frame.
#define LOG_INFO(fmt, ...)  do { logTimestamp(stdout); std::printf("[INFO]  " fmt "\n", ##__VA_ARGS__); } while (0)
#define LOG_WARN(fmt, ...)  do { logTimestamp(stderr); std::fprintf(stderr, "[WARN]  " fmt "\n", ##__VA_ARGS__); } while (0)
#define LOG_ERROR(fmt, ...) do { logTimestamp(stderr); std::fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__); } while (0)

// Equivalent iostream de logPrintf pour les fichiers qui utilisent << :
// remplace std::cout par logOut() et std::cerr par logErr() pour horodater
// chaque ligne. Retourne le flux pour enchainer les << comme avant.
inline std::ostream& logOut() { logTimestamp(stdout); return std::cout; }
inline std::ostream& logErr() { logTimestamp(stderr); return std::cerr; }

// Imprime un bloc deja formaté (multi-lignes) avec UN seul horodatage en tete.
// Evite d'horodater le milieu d'un arbre/dump multi-lignes construit par
// plusieurs appels successifs.
inline void logRaw(const std::string& text) {
    logTimestamp(stdout);
    std::fwrite(text.c_str(), 1, text.size(), stdout);
}
