#include "PokemonEcology.h"

#include <cmath>
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

void expectNear(float actual, float expected, const std::string &message)
{
	if (std::fabs(actual - expected) > 0.0001f)
	{
		std::cerr << "FAIL: " << message << " (expected " << expected
		          << ", got " << actual << ')' << std::endl;
		++failures;
	}
}

void testUmbreonTerritoryExpandsAtNight()
{
	const PokemonEcologySample day =
		samplePokemonEcology(PokemonSpecies::Umbreon, 1.0f);
	const PokemonEcologySample night =
		samplePokemonEcology(PokemonSpecies::Umbreon, 0.0f);
	expectTrue(night.visionRangeScale > day.visionRangeScale &&
	               night.hearingRangeScale > day.hearingRangeScale &&
	               night.alertGainScale > day.alertGainScale,
	           "Umbreon detects intruders across a wider territory at night");
	expectTrue(night.alertDecayScale < day.alertDecayScale,
	           "Umbreon stays alert longer at night");
}

void testDiurnalGroundSpeciesMoveMoreByDay()
{
	const PokemonEcologySample bulbasaurDay =
		samplePokemonEcology(PokemonSpecies::Bulbasaur, 1.0f);
	const PokemonEcologySample bulbasaurNight =
		samplePokemonEcology(PokemonSpecies::Bulbasaur, 0.0f);
	const PokemonEcologySample eeveeDay =
		samplePokemonEcology(PokemonSpecies::Eevee, 1.0f);
	const PokemonEcologySample eeveeNight =
		samplePokemonEcology(PokemonSpecies::Eevee, 0.0f);
	expectTrue(bulbasaurDay.wanderSpeedScale > bulbasaurNight.wanderSpeedScale,
	           "Bulbasaur is more active in daylight");
	expectTrue(eeveeDay.wanderSpeedScale > eeveeNight.wanderSpeedScale,
	           "Eevee is more active in daylight");
}

void testTwilightBlendsContinuouslyAndInputsClamp()
{
	const PokemonEcologySample night =
		samplePokemonEcology(PokemonSpecies::Umbreon, 0.0f);
	const PokemonEcologySample day =
		samplePokemonEcology(PokemonSpecies::Umbreon, 1.0f);
	const PokemonEcologySample twilight =
		samplePokemonEcology(PokemonSpecies::Umbreon, 0.5f);
	expectNear(twilight.visionRangeScale,
	           (night.visionRangeScale + day.visionRangeScale) * 0.5f,
	           "twilight linearly blends the night and day profiles");
	expectNear(samplePokemonEcology(PokemonSpecies::Umbreon, -2.0f)
	               .visionRangeScale,
	           night.visionRangeScale, "negative daylight clamps to night");
	expectNear(samplePokemonEcology(PokemonSpecies::Umbreon, 3.0f)
	               .visionRangeScale,
	           day.visionRangeScale, "excess daylight clamps to day");
}

void testWildCharizardRemainsNeutralInThisSlice()
{
	const PokemonEcologySample day =
		samplePokemonEcology(PokemonSpecies::Charizard, 1.0f);
	const PokemonEcologySample night =
		samplePokemonEcology(PokemonSpecies::Charizard, 0.0f);
	expectNear(day.wanderSpeedScale, 1.0f,
	           "Charizard day activity remains unchanged");
	expectNear(night.visionRangeScale, 1.0f,
	           "Charizard night perception remains unchanged");
}

void testDayAndNightChangeTheEncounterPoolDeterministically()
{
	int umbreonDay = 0;
	int umbreonNight = 0;
	int bulbasaurDay = 0;
	int bulbasaurNight = 0;
	int eeveeDay = 0;
	int eeveeNight = 0;
	for (int index = 0; index < 48; ++index)
	{
		const PokemonSpecies species = groundPokemonSpeciesForIndex(index);
		const bool dayPresent = pokemonEcologySlotPresent(species, index, 1.0f);
		const bool nightPresent = pokemonEcologySlotPresent(species, index, 0.0f);
		if (species == PokemonSpecies::Umbreon)
		{
			umbreonDay += dayPresent ? 1 : 0;
			umbreonNight += nightPresent ? 1 : 0;
		}
		else if (species == PokemonSpecies::Bulbasaur)
		{
			bulbasaurDay += dayPresent ? 1 : 0;
			bulbasaurNight += nightPresent ? 1 : 0;
		}
		else if (species == PokemonSpecies::Eevee)
		{
			eeveeDay += dayPresent ? 1 : 0;
			eeveeNight += nightPresent ? 1 : 0;
		}
	}
	expectTrue(umbreonNight > umbreonDay && umbreonDay > 0,
	           "night exposes more Umbreon while retaining one daytime encounter");
	expectTrue(bulbasaurDay > bulbasaurNight && bulbasaurNight > 0,
	           "day exposes more Bulbasaur while retaining night discoveries");
	expectTrue(eeveeDay > eeveeNight && eeveeNight > 0,
	           "day exposes more Eevee while retaining night discoveries");
	for (int index = 0; index < 8; ++index)
	{
		expectTrue(pokemonEcologySlotPresent(
		               PokemonSpecies::Charizard, index, 0.0f) &&
		               pokemonEcologySlotPresent(
		                   PokemonSpecies::Charizard, index, 1.0f),
		           "wild Charizard remains present across the first weather slice");
	}
	expectTrue(pokemonEcologySlotPresent(PokemonSpecies::Umbreon, 7, -2.0f) ==
	               pokemonEcologySlotPresent(PokemonSpecies::Umbreon, 7, 0.0f) &&
	               pokemonEcologySlotPresent(PokemonSpecies::Umbreon, 7, 3.0f) ==
	               pokemonEcologySlotPresent(PokemonSpecies::Umbreon, 7, 1.0f),
	           "presence rules clamp invalid daylight values");
}

void testFieldFeedbackMatchesTheEcologyBands()
{
	expectTrue(std::string(pokemonEcologyPhaseName(1.0f)) == "Day" &&
	               std::string(pokemonEcologyFieldHint(1.0f)).find("Eevee") !=
	                   std::string::npos,
	           "day feedback names the active diurnal species");
	expectTrue(std::string(pokemonEcologyPhaseName(0.5f)) == "Twilight" &&
	               std::string(pokemonEcologyFieldHint(0.5f)).find("shifting") !=
	                   std::string::npos,
	           "twilight feedback explains the transition");
	expectTrue(std::string(pokemonEcologyPhaseName(0.0f)) == "Night" &&
	               std::string(pokemonEcologyFieldHint(0.0f)).find("Umbreon") !=
	                   std::string::npos,
	           "night feedback warns about expanded Umbreon territory");
}
}

int main()
{
	testUmbreonTerritoryExpandsAtNight();
	testDiurnalGroundSpeciesMoveMoreByDay();
	testTwilightBlendsContinuouslyAndInputsClamp();
	testWildCharizardRemainsNeutralInThisSlice();
	testDayAndNightChangeTheEncounterPoolDeterministically();
	testFieldFeedbackMatchesTheEcologyBands();
	if (failures != 0)
	{
		return 1;
	}
	std::cout << "Pokemon ecology tests passed" << std::endl;
	return 0;
}
