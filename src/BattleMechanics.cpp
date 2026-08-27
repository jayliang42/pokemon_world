#include "BattleMechanics.h"

#include <algorithm>
#include <cmath>
#include <limits>

BattleStats battleStatsFor(PokemonSpecies species)
{
	switch (species)
	{
	case PokemonSpecies::Umbreon:
		return {96, 65, 110};
	case PokemonSpecies::Bulbasaur:
		return {82, 49, 49};
	case PokemonSpecies::Eevee:
		return {82, 55, 50};
	case PokemonSpecies::Charizard:
		return {118, 84, 78};
	}
	return BattleStats();
}

BattleMove playerBattleMove()
{
	return playerBattleMoves()[0];
}

const std::array<BattleMove, PLAYER_MOVE_SLOT_COUNT> &playerBattleMoves()
{
	static const std::array<BattleMove, PLAYER_MOVE_SLOT_COUNT> moves = {{
		{BattleMoveId::Ember, "Ember", PokemonType::Fire, 28, 2.8f,
		 {0.18f, 0.30f, 0.18f, 0.08f, 0.20f}},
		{BattleMoveId::AirSlash, "Air Slash", PokemonType::Flying, 34, 4.6f,
		 {0.32f, 0.40f, 0.36f, 0.16f, 0.55f}},
		{BattleMoveId::Flamethrower, "Flamethrower", PokemonType::Fire, 42, 7.2f,
		 {0.52f, 0.68f, 0.72f, 0.26f, 1.0f}},
	}};
	return moves;
}

BattleMove wildBattleMoveFor(PokemonSpecies species)
{
	switch (species)
	{
	case PokemonSpecies::Umbreon:
		return {BattleMoveId::Bite, "Bite", PokemonType::Dark, 24, 0.0f,
		        {0.48f, 0.20f, 0.32f, 0.18f, 1.0f}};
	case PokemonSpecies::Bulbasaur:
		return {BattleMoveId::VineWhip, "Vine Whip", PokemonType::Grass, 26, 0.0f,
		        {0.55f, 0.44f, 0.30f, 0.14f, 1.0f}};
	case PokemonSpecies::Eevee:
		return {BattleMoveId::Tackle, "Tackle", PokemonType::Normal, 23, 0.0f,
		        {0.38f, 0.28f, 0.25f, 0.10f, 1.0f}};
	case PokemonSpecies::Charizard:
		return {BattleMoveId::WingAttack, "Wing Attack", PokemonType::Flying, 30, 0.0f,
		        {0.50f, 0.42f, 0.35f, 0.18f, 1.0f}};
	}
	return BattleMove();
}

bool validateBattleMoveTiming(const BattleMoveTiming &timing)
{
	return std::isfinite(timing.startupSeconds) && timing.startupSeconds > 0.0f &&
	       std::isfinite(timing.activeSeconds) && timing.activeSeconds > 0.0f &&
	       std::isfinite(timing.recoverySeconds) && timing.recoverySeconds > 0.0f &&
	       std::isfinite(timing.staggerSeconds) && timing.staggerSeconds >= 0.0f &&
	       std::isfinite(timing.movementLock) && timing.movementLock >= 0.0f &&
	       timing.movementLock <= 1.0f;
}

float battleTypeEffectiveness(PokemonType attackType,
	                          PokemonSpecies defenderSpecies)
{
	if (defenderSpecies == PokemonSpecies::Bulbasaur)
	{
		if (attackType == PokemonType::Fire || attackType == PokemonType::Flying)
		{
			return 2.0f;
		}
		if (attackType == PokemonType::Grass)
		{
			return 0.5f;
		}
	}
	else if (defenderSpecies == PokemonSpecies::Charizard)
	{
		if (attackType == PokemonType::Fire || attackType == PokemonType::Grass)
		{
			return 0.5f;
		}
	}
	else if (defenderSpecies == PokemonSpecies::Umbreon &&
	         attackType == PokemonType::Dark)
	{
		return 0.5f;
	}
	return 1.0f;
}

bool moveMatchesSpecies(PokemonType moveType, PokemonSpecies species)
{
	switch (species)
	{
	case PokemonSpecies::Umbreon:
		return moveType == PokemonType::Dark;
	case PokemonSpecies::Bulbasaur:
		return moveType == PokemonType::Grass;
	case PokemonSpecies::Eevee:
		return moveType == PokemonType::Normal;
	case PokemonSpecies::Charizard:
		return moveType == PokemonType::Fire || moveType == PokemonType::Flying;
	}
	return false;
}

BattleDamageResult resolveBattleDamage(PokemonSpecies attackerSpecies,
	                                    PokemonSpecies defenderSpecies,
	                                    const BattleMove &move)
{
	const BattleStats attacker = battleStatsFor(attackerSpecies);
	const BattleStats defender = battleStatsFor(defenderSpecies);
	BattleDamageResult result;
	result.effectiveness = battleTypeEffectiveness(move.type, defenderSpecies);
	result.sameTypeBonus = moveMatchesSpecies(move.type, attackerSpecies);
	const float safeDefense = static_cast<float>(std::max(1, defender.defense));
	const float baseDamage = 2.0f +
	                         static_cast<float>(std::max(1, move.power)) *
	                             static_cast<float>(std::max(1, attacker.attack)) /
	                             safeDefense * 0.45f;
	const float sameTypeMultiplier = result.sameTypeBonus ? 1.2f : 1.0f;
	result.amount = std::max(
		1, static_cast<int>(std::round(baseDamage * sameTypeMultiplier *
		                               result.effectiveness)));
	return result;
}

PlayerHitResult resolvePlayerHit(int currentHealth, int incomingDamage,
	                             bool invulnerable)
{
	PlayerHitResult result;
	result.remainingHealth = std::max(0, currentHealth);
	if (invulnerable)
	{
		result.evaded = true;
		return result;
	}

	result.appliedDamage = std::min(result.remainingHealth,
	                                std::max(0, incomingDamage));
	result.remainingHealth -= result.appliedDamage;
	return result;
}

bool isPerfectDodge(bool evaded, float secondsSinceDodgeStarted)
{
	return evaded && std::isfinite(secondsSinceDodgeStarted) &&
	       secondsSinceDodgeStarted >= PERFECT_DODGE_MIN_SECONDS &&
	       secondsSinceDodgeStarted <= PERFECT_DODGE_MAX_SECONDS;
}

int perfectCounterDamage(int baseDamage)
{
	if (baseDamage <= 0)
	{
		return 0;
	}
	const long long boosted = std::llround(
		static_cast<double>(baseDamage) *
		static_cast<double>(PERFECT_COUNTER_DAMAGE_MULTIPLIER));
	return static_cast<int>(std::min(
		boosted, static_cast<long long>(std::numeric_limits<int>::max())));
}
