#include "Game.h"
#include "SelfTest.h"

#include <cstring>

/*
* TODO:
*   Collision (Hitbox)
*   Multi
*   3D Models roughness
*/

int main(int argc, char* argv[]) {
    // Auto-tests de la logique pure, sans fenetre : OpenGLProject.exe -selftest
    for (int i = 1; i < argc; ++i) {
        if (argv[i] && std::strcmp(argv[i], "-selftest") == 0) return runSelfTests();
        // Outil de contenu (squelette Mixamo -> JSON) : -dumprig <entree> <sortie.json>
        if (argv[i] && std::strcmp(argv[i], "-dumprig") == 0 && i + 2 < argc) return dumpRig(argv[i + 1], argv[i + 2]);
    }

    Game* game = new Game(argc, argv);

    game->run();

    delete game;
    
    return 0;
}