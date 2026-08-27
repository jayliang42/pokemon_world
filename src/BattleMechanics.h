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
	Tackle,
	WingAttack,
};

struct BattleStats
{
	int maximumHealth = 1;
	int attack = 1;
	int defense = 1;
};

struct BattleMoveTiming
{
	float startupSeconds = 0.22f;
	float activeSeconds = 0.46f;
	float recoverySeconds = 0.42f;
	float staggerSeconds = 0.12f;
	float movementLock = 1.0f;
};

struct BattleMove
{
	BattleMoveId id = BattleMoveId::Ember;
	const char *name = "Ember";
	PokemonType type = PokemonType::Fire;
	int power = 1;
	float cooldownSeconds = 1.0f;
	BattleMoveTiming timing;
};

struct BattleDamageResult
{
	int amount = 1;
	float effectiveness = 1.0f;
	bool sameTypeBonus = false;
};

struct PlayerHitResult
{
	int appliedDamage = 0;
	int remainingHealth = 0;
	bool evaded = false;
};

BattleStats battleStatsFor(PokemonSpecies species);
constexpr int PLAYER_MOVE_SLOT_COUNT = 3;
constexpr float PERFECT_DODGE_MIN_SECONDS = 0.08f;
constexpr float PERFECT_DODGE_MAX_SECONDS = 0.32f;
constexpr float PERFECT_COUNTER_WINDOW_SECONDS = 1.6f;
constexpr float PERFECT_COUNTER_DAMAGE_MULTIPLIER = 1.35f;
constexpr float PERFECT_COUNTER_STARTUP_MULTIPLIER = 0.55f;
const std::array<BattleMove, PLAYER_MOVE_SLOT_COUNT> &playerBattleMoves();
BattleMove playerBattleMove();
BattleMove wildBattleMoveFor(PokemonSpecies species);
bool validateBattleMoveTiming(const BattleMoveTiming &timing);
float battleTypeEffectiveness(PokemonType attackType,
	                          PokemonSpecies defenderSpecies);
bool moveMatchesSpecies(PokemonType moveType, PokemonSpecies species);
BattleDamageResult resolveBattleDamage(PokemonSpecies attackerSpecies,
	                                    PokemonSpecies defenderSpecies,
	                                    const BattleMove &move);
PlayerHitResult resolvePlayerHit(int currentHealth, int incomingDamage,
	                             bool invulnerable);
bool isPerfectDodge(bool evaded, float secondsSinceDodgeStarted);
int perfectCounterDamage(int baseDamage);
