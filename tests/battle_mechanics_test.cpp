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
	const BattleStats charizard = battleStatsFor(PokemonSpecies::Charizard);
	expectTrue(umbreon.defense > charizard.defense &&
	               charizard.defense > bulbasaur.defense,
	           "species preserve distinct defensive identities");
	expectTrue(charizard.maximumHealth > bulbasaur.maximumHealth,
	           "larger species receive a larger health pool");
	expectTrue(wildBattleMoveFor(PokemonSpecies::Umbreon).id == BattleMoveId::Bite,
	           "Umbreon uses Bite as its counter move");
	expectTrue(wildBattleMoveFor(PokemonSpecies::Bulbasaur).id ==
	               BattleMoveId::VineWhip,
	           "Bulbasaur uses Vine Whip as its counter move");
	expectTrue(wildBattleMoveFor(PokemonSpecies::Charizard).id ==
	               BattleMoveId::WingAttack,
	           "Charizard uses Wing Attack as its counter move");
}

void testTypeMatchAndEffectiveness()
{
	expectTrue(moveMatchesSpecies(PokemonType::Fire, PokemonSpecies::Charizard),
	           "Charizard receives same-type bonus for Fire moves");
	expectTrue(moveMatchesSpecies(PokemonType::Flying, PokemonSpecies::Charizard),
	           "Charizard receives same-type bonus for Flying moves");
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
}

int main()
{
	testSpeciesStatsAndMovesAreDistinct();
	testTypeMatchAndEffectiveness();
	testDamageIsDeterministicAndRespectsMatchups();
	testDamageNeverFallsBelowOne();

	if (failures != 0)
	{
		std::cerr << failures << " battle mechanic test(s) failed" << std::endl;
		return 1;
	}

	std::cout << "All battle mechanic tests passed" << std::endl;
	return 0;
}

