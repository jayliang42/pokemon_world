#pragma once

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
};

struct BattleDamageResult
{
	int amount = 1;
	float effectiveness = 1.0f;
	bool sameTypeBonus = false;
};

BattleStats battleStatsFor(PokemonSpecies species);
BattleMove playerBattleMove();
BattleMove wildBattleMoveFor(PokemonSpecies species);
float battleTypeEffectiveness(PokemonType attackType,
	                          PokemonSpecies defenderSpecies);
bool moveMatchesSpecies(PokemonType moveType, PokemonSpecies species);
BattleDamageResult resolveBattleDamage(PokemonSpecies attackerSpecies,
	                                    PokemonSpecies defenderSpecies,
	                                    const BattleMove &move);

