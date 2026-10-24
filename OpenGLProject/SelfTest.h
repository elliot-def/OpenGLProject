#pragma once

// Auto-tests de la logique pure (sans fenetre ni OpenGL) : collisions (rayon,
// sphere + grille de triangles), A* de la creature, options de partie.
// Lancement : OpenGLProject.exe -selftest (code de retour 0 = tout passe).
int runSelfTests();

// Outil de contenu : exporte en JSON la hierarchie des noeuds (transformations
// locales de repos) d'un fichier lu par Assimp, comme les animations externes
// (pivots FBX non conserves). Sert a res/models/fropy/make_fropy_player.py.
// Lancement : OpenGLProject.exe -dumprig <entree.fbx> <sortie.json>
int dumpRig(const char* inPath, const char* outPath);
