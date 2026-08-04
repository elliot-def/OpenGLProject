#include "Game.h"

/*
* TODO:
*   Collision (Hitbox)
*   Multi
*   3D Models roughness
*/

int main(int argc, char* argv[]) {
    Game* game = new Game(argc, argv);

    game->run();

    delete game;
    
    return 0;
}