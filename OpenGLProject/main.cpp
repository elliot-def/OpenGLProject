#include "Game.h"

/*
* TODO:
*   -> Refonte de la classe texture pour ne plus prendre differencier les usages (cube: avec details.json, normal etc, et model: normal.png, diffuse.jpg, specular.jpg)
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