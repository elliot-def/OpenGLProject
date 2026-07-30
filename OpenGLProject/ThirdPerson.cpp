#include "ThirdPerson.h"

ThirdPerson::ThirdPerson(Player* player) : Key(player, "ThirdPerson", ConfigKeys::KEY_THIRD_PERSON) {
	setOnReleaseAction(InputContext::GAME, [this]() {
		m_player->processThirdPersonKey();
	});
}
