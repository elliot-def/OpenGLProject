#include "Game.h"

/*
* TODO:
*   Collision (Hitbox)
*   Multi
*   3D Models roughness
*/

int main() {
    Game* game = new Game();

    game->run();

    delete game;
    
    return 0;
}