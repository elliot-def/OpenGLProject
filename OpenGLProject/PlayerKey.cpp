#include "PlayerKey.h"

PlayerKey::PlayerKey(Player* player, const std::string& name, int key,
                     Action onPress, Action onRelease, Action ifPressed)
    : Key(player, name, key)
{
    if (onPress)
        setOnPressAction(InputContext::GAME, std::move(onPress));
    if (onRelease)
        setOnReleaseAction(InputContext::GAME, std::move(onRelease));
    if (ifPressed)
        setIfPressedAction(InputContext::GAME, std::move(ifPressed));
}
