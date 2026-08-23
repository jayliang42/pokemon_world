#pragma once

#include <array>

#include "PokemonSpecies.h"

enum class PokemonType
{
	Normal,
	Fire,
	Grass,
	Flying,
	Dark,
};

enum class BattleMoveId
{
	Ember,
	AirSlash,
	Flamethrower,
	VineWhip,
	Bite,
	WingAttack,
};

struct BattleStats
{
	int maximumHealth = 1;
	int attack = 1;
	int defense = 1;
};

struct BattleMove
{
	BattleMoveId id = BattleMoveId::Ember;
	const char *name = "Ember";
	PokemonType type = PokemonType::Fire;
	int power = 1;
	float cooldownSeconds = 1.0f;
};

struct BattleDamageResult
{
	int amount = 1;
	float effectiveness = 1.0f;
	bool sameTypeBonus = false;
};

BattleStats battleStatsFor(PokemonSpecies species);
constexpr int PLAYER_MOVE_SLOT_COUNT = 3;
const std::array<BattleMove, PLAYER_MOVE_SLOT_COUNT> &playerBattleMoves();
BattleMove playerBattleMove();
BattleMove wildBattleMoveFor(PokemonSpecies species);
float battleTypeEffectiveness(PokemonType attackType,
	                          PokemonSpecies defenderSpecies);
bool moveMatchesSpecies(PokemonType moveType, PokemonSpecies species);
BattleDamageResult resolveBattleDamage(PokemonSpecies attackerSpecies,
	                                    PokemonSpecies defenderSpecies,
	                                    const BattleMove &move);
