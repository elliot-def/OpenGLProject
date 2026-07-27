#pragma once

#include "Jump.h"

Jump::Jump(Player* player) : Key(player, "Jump", ConfigKeys::KEY_JUMP) {
	// Le saut est une impulsion unique à l'appui de la touche (et non continue
	// tant qu'elle est maintenue), sinon le joueur "collerait" au plafond en
	// accumulant du Y chaque frame.
	setOnPressAction(InputContext::GAME, [this]() {
		m_player->processJump();
	});
}