#include "Game.h"

/*
* TODO:
*   Collision (Hitbox)
*   Multi
*   Son (fonction pour mute quand plus le focus glfwSetCursorEnterCallback(window, cursor_enter_callback);)
*/

int main() {
    Game* game = new Game();

    game->run();

    delete game;
    
    return 0;
}