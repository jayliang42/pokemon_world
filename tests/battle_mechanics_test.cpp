#include "BattleMechanics.h"

#include <iostream>
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

void testSpeciesStatsAndMovesAreDistinct()
{
	const BattleStats umbreon = battleStatsFor(PokemonSpecies::Umbreon);
	const BattleStats bulbasaur = battleStatsFor(PokemonSpecies::Bulbasaur);
	const BattleStats eevee = battleStatsFor(PokemonSpecies::Eevee);
	const BattleStats charizard = battleStatsFor(PokemonSpecies::Charizard);
	expectTrue(umbreon.defense > charizard.defense &&
	               charizard.defense > bulbasaur.defense,
	           "species preserve distinct defensive identities");
	expectTrue(charizard.maximumHealth > bulbasaur.maximumHealth,
	           "larger species receive a larger health pool");
	expectTrue(eevee.maximumHealth == bulbasaur.maximumHealth &&
	               eevee.attack > bulbasaur.attack,
	           "Eevee preserves the ground save health limit with its own attack identity");
	expectTrue(wildBattleMoveFor(PokemonSpecies::Umbreon).id == BattleMoveId::Bite,
	           "Umbreon uses Bite as its counter move");
	expectTrue(wildBattleMoveFor(PokemonSpecies::Bulbasaur).id ==
	               BattleMoveId::VineWhip,
	           "Bulbasaur uses Vine Whip as its counter move");
	expectTrue(wildBattleMoveFor(PokemonSpecies::Eevee).id == BattleMoveId::Tackle,
	           "Eevee uses Tackle as its counter move");
	expectTrue(wildBattleMoveFor(PokemonSpecies::Charizard).id ==
	               BattleMoveId::WingAttack,
	           "Charizard uses Wing Attack as its counter move");
	expectTrue(playerBattleMoves()[1].id == BattleMoveId::AirSlash &&
	               playerBattleMoves()[2].id == BattleMoveId::Flamethrower,
	           "the player loadout exposes the additional Charizard moves");
}

void testTypeMatchAndEffectiveness()
{
	expectTrue(moveMatchesSpecies(PokemonType::Fire, PokemonSpecies::Charizard),
	           "Charizard receives same-type bonus for Fire moves");
	expectTrue(moveMatchesSpecies(PokemonType::Flying, PokemonSpecies::Charizard),
	           "Charizard receives same-type bonus for Flying moves");
	expectTrue(moveMatchesSpecies(PokemonType::Normal, PokemonSpecies::Eevee),
	           "Eevee receives same-type bonus for Normal moves");
	expectTrue(battleTypeEffectiveness(PokemonType::Fire,
	                                  PokemonSpecies::Bulbasaur) == 2.0f,
	           "Fire is super effective against Bulbasaur");
	expectTrue(battleTypeEffectiveness(PokemonType::Grass,
	                                  PokemonSpecies::Charizard) == 0.5f,
	           "Charizard resists Grass");
	expectTrue(battleTypeEffectiveness(PokemonType::Normal,
	                                  PokemonSpecies::Umbreon) == 1.0f,
	           "unlisted matchups remain neutral");
}

void testDamageIsDeterministicAndRespectsMatchups()
{
	const BattleMove ember = playerBattleMove();
	const BattleDamageResult first = resolveBattleDamage(
		PokemonSpecies::Charizard, PokemonSpecies::Bulbasaur, ember);
	const BattleDamageResult second = resolveBattleDamage(
		PokemonSpecies::Charizard, PokemonSpecies::Bulbasaur, ember);
	const BattleDamageResult resisted = resolveBattleDamage(
		PokemonSpecies::Charizard, PokemonSpecies::Charizard, ember);
	expectTrue(first.amount == second.amount,
	           "equal attacks resolve to deterministic damage");
	expectTrue(first.amount > resisted.amount,
	           "super-effective Ember deals more damage than a resisted Ember");
	expectTrue(first.effectiveness == 2.0f && first.sameTypeBonus,
	           "damage result exposes effectiveness and same-type feedback");
}

void testDamageNeverFallsBelowOne()
{
	BattleMove weakMove;
	weakMove.type = PokemonType::Normal;
	weakMove.power = 0;
	const BattleDamageResult result = resolveBattleDamage(
		PokemonSpecies::Bulbasaur, PokemonSpecies::Umbreon, weakMove);
	expectTrue(result.amount >= 1,
	           "invalid or tiny move power still produces bounded chip damage");
}

void testPlayerHitAppliesBoundedDamage()
{
	const PlayerHitResult hit = resolvePlayerHit(30, 12, false);
	expectTrue(hit.appliedDamage == 12 && hit.remainingHealth == 18 &&
	               !hit.evaded,
	           "ordinary player hit applies damage and reports remaining health");

	const PlayerHitResult knockout = resolvePlayerHit(8, 40, false);
	expectTrue(knockout.appliedDamage == 8 && knockout.remainingHealth == 0,
	           "player hit cannot apply more damage than remaining health");

	const PlayerHitResult invalid = resolvePlayerHit(20, -4, false);
	expectTrue(invalid.appliedDamage == 0 && invalid.remainingHealth == 20,
	           "negative incoming damage cannot heal or hurt the player");
}

void testInvulnerabilityEvadesPlayerHit()
{
	const PlayerHitResult result = resolvePlayerHit(30, 18, true);
	expectTrue(result.evaded, "invulnerability marks the incoming hit as evaded");
	expectTrue(result.appliedDamage == 0 && result.remainingHealth == 30,
	           "invulnerability prevents both damage and health loss");
}
}

int main()
{
	testSpeciesStatsAndMovesAreDistinct();
	testTypeMatchAndEffectiveness();
	testDamageIsDeterministicAndRespectsMatchups();
	testDamageNeverFallsBelowOne();
	testPlayerHitAppliesBoundedDamage();
	testInvulnerabilityEvadesPlayerHit();

	if (failures != 0)
	{
		std::cerr << failures << " battle mechanic test(s) failed" << std::endl;
		return 1;
	}

	std::cout << "All battle mechanic tests passed" << std::endl;
	return 0;
}
