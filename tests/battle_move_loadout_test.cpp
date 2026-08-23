#include "BattleMoveLoadout.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace
{
int failures = 0;

void expectTrue(bool condition, const std::string &message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << std::endl;
		++failures;
	}
}

void expectNear(double actual, double expected, double tolerance,
	            const std::string &message)
{
	if (std::fabs(actual - expected) > tolerance)
	{
		std::cerr << "FAIL: " << message << " (expected " << expected
		          << ", got " << actual << ")" << std::endl;
		++failures;
	}
}

void testPlayerLoadoutHasDistinctTradeoffs()
{
	const auto &moves = playerBattleMoves();
	expectTrue(moves.size() == PLAYER_MOVE_SLOT_COUNT,
	           "Charizard exposes exactly three player move slots");
	expectTrue(moves[0].id == BattleMoveId::Ember &&
	               moves[1].id == BattleMoveId::AirSlash &&
	               moves[2].id == BattleMoveId::Flamethrower,
	           "move slots remain stable for keyboard and HUD mapping");
	expectTrue(moves[0].power < moves[1].power &&
	               moves[1].power < moves[2].power,
	           "stronger moves trade speed for higher damage");
	expectTrue(moves[0].cooldownSeconds < moves[1].cooldownSeconds &&
	               moves[1].cooldownSeconds < moves[2].cooldownSeconds,
	           "stronger moves have longer cooldowns");
	expectTrue(moves[1].type == PokemonType::Flying &&
	               moves[0].type == PokemonType::Fire &&
	               moves[2].type == PokemonType::Fire,
	           "the loadout offers both Flying and Fire coverage");
}

void testSelectionRejectsInvalidSlotsWithoutMutation()
{
	BattleMoveLoadout loadout;
	expectTrue(loadout.selectedSlot() == 0 &&
	               loadout.selectedMove().id == BattleMoveId::Ember,
	           "Ember is the safe default move");
	expectTrue(loadout.selectSlot(2) &&
	               loadout.selectedMove().id == BattleMoveId::Flamethrower,
	           "a valid slot changes the selected move");
	expectTrue(!loadout.selectSlot(-1) && !loadout.selectSlot(3) &&
	               loadout.selectedSlot() == 2,
	           "invalid slots are rejected without changing selection");
}

void testCooldownConsumptionAndExpiry()
{
	BattleMoveLoadout loadout;
	loadout.selectSlot(1);
	expectTrue(loadout.canUseSelected(10.0),
	           "a fresh selected move is immediately available");
	expectTrue(loadout.consumeSelected(10.0),
	           "using an available move starts its cooldown");
	expectTrue(!loadout.canUseSelected(10.1) &&
	               !loadout.consumeSelected(10.1),
	           "a cooling move cannot be consumed twice");
	expectNear(loadout.cooldownRemaining(1, 12.0), 2.6, 0.0001,
	           "remaining cooldown uses monotonic game time");
	expectNear(loadout.cooldownFraction(1, 12.0), 2.6 / 4.6, 0.0001,
	           "HUD fraction matches remaining cooldown duration");
	expectTrue(loadout.canUseSelected(14.6),
	           "the move becomes available at its cooldown boundary");
}

void testOtherSlotsStayAvailableAndResetClearsState()
{
	BattleMoveLoadout loadout;
	loadout.selectSlot(2);
	loadout.consumeSelected(4.0);
	loadout.selectSlot(0);
	expectTrue(loadout.canUseSelected(4.1),
	           "cooldowns are tracked independently per slot");
	loadout.reset();
	expectTrue(loadout.selectedSlot() == 0 &&
	               loadout.cooldownRemaining(2, 4.1) == 0.0,
	           "new runs restore the default selection and clear cooldowns");
}

void testInvalidTimesFailClosed()
{
	BattleMoveLoadout loadout;
	expectTrue(!loadout.consumeSelected(-1.0) &&
	               !loadout.consumeSelected(
	                   std::numeric_limits<double>::quiet_NaN()),
	           "invalid clocks cannot consume a move");
	expectTrue(loadout.cooldownRemaining(-1, 1.0) == 0.0 &&
	               loadout.cooldownFraction(99, 1.0) == 0.0f,
	           "invalid HUD slot queries remain bounded");
}
}

int main()
{
	testPlayerLoadoutHasDistinctTradeoffs();
	testSelectionRejectsInvalidSlotsWithoutMutation();
	testCooldownConsumptionAndExpiry();
	testOtherSlotsStayAvailableAndResetClearsState();
	testInvalidTimesFailClosed();

	if (failures != 0)
	{
		std::cerr << failures << " battle move loadout check(s) failed"
		          << std::endl;
		return 1;
	}
	std::cout << "Battle move loadout checks passed" << std::endl;
	return 0;
}
