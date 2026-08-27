#pragma once

#include "PokemonSpecies.h"

struct PokemonEcologySample
{
	float wanderSpeedScale = 1.0f;
	float visionRangeScale = 1.0f;
	float hearingRangeScale = 1.0f;
	float alertGainScale = 1.0f;
	float alertDecayScale = 1.0f;
};

PokemonEcologySample samplePokemonEcology(PokemonSpecies species,
	                                       float daylight);
float pokemonEcologyPresenceFraction(PokemonSpecies species, float daylight);
bool pokemonEcologySlotPresent(PokemonSpecies species, int pokemonId,
	                           float daylight);
const char *pokemonEcologyPhaseName(float daylight);
const char *pokemonEcologyFieldHint(float daylight);
