#pragma once

#include <array>

#include "BattleMechanics.h"

class BattleMoveLoadout
{
public:
	BattleMoveLoadout();

	bool selectSlot(int slot);
	int selectedSlot() const;
	const BattleMove &selectedMove() const;

	bool canUseSelected(double now) const;
	bool consumeSelected(double now);
	double cooldownRemaining(int slot, double now) const;
	float cooldownFraction(int slot, double now) const;
	void reset();

private:
	int selectedSlot_ = 0;
	std::array<double, PLAYER_MOVE_SLOT_COUNT> readyAt_;
};
