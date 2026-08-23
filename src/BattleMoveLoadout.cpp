#include "BattleMoveLoadout.h"

#include <algorithm>
#include <cmath>

namespace
{
bool isValidSlot(int slot)
{
	return slot >= 0 && slot < PLAYER_MOVE_SLOT_COUNT;
}

bool isValidTime(double now)
{
	return std::isfinite(now) && now >= 0.0;
}
}

BattleMoveLoadout::BattleMoveLoadout()
{
	reset();
}

bool BattleMoveLoadout::selectSlot(int slot)
{
	if (!isValidSlot(slot))
	{
		return false;
	}
	selectedSlot_ = slot;
	return true;
}

int BattleMoveLoadout::selectedSlot() const
{
	return selectedSlot_;
}

const BattleMove &BattleMoveLoadout::selectedMove() const
{
	return playerBattleMoves()[static_cast<std::size_t>(selectedSlot_)];
}

bool BattleMoveLoadout::canUseSelected(double now) const
{
	return isValidTime(now) && cooldownRemaining(selectedSlot_, now) <= 0.0;
}

bool BattleMoveLoadout::consumeSelected(double now)
{
	if (!canUseSelected(now))
	{
		return false;
	}
	readyAt_[static_cast<std::size_t>(selectedSlot_)] =
		now + std::max(0.0f, selectedMove().cooldownSeconds);
	return true;
}

double BattleMoveLoadout::cooldownRemaining(int slot, double now) const
{
	if (!isValidSlot(slot) || !isValidTime(now))
	{
		return 0.0;
	}
	return std::max(
		0.0, readyAt_[static_cast<std::size_t>(slot)] - now);
}

float BattleMoveLoadout::cooldownFraction(int slot, double now) const
{
	if (!isValidSlot(slot))
	{
		return 0.0f;
	}
	const float duration =
		playerBattleMoves()[static_cast<std::size_t>(slot)].cooldownSeconds;
	if (duration <= 0.0f)
	{
		return 0.0f;
	}
	return static_cast<float>(std::min(
		1.0, cooldownRemaining(slot, now) / static_cast<double>(duration)));
}

void BattleMoveLoadout::reset()
{
	selectedSlot_ = 0;
	readyAt_.fill(0.0);
}
