#include "PokemonEcology.h"

#include <algorithm>
#include <cstdint>

namespace
{
PokemonEcologySample mixSamples(const PokemonEcologySample &night,
	                             const PokemonEcologySample &day,
	                             float daylight)
{
	const float dayWeight = std::max(0.0f, std::min(1.0f, daylight));
	const float nightWeight = 1.0f - dayWeight;
	PokemonEcologySample sample;
	sample.wanderSpeedScale =
		night.wanderSpeedScale * nightWeight + day.wanderSpeedScale * dayWeight;
	sample.visionRangeScale =
		night.visionRangeScale * nightWeight + day.visionRangeScale * dayWeight;
	sample.hearingRangeScale =
		night.hearingRangeScale * nightWeight + day.hearingRangeScale * dayWeight;
	sample.alertGainScale =
		night.alertGainScale * nightWeight + day.alertGainScale * dayWeight;
	sample.alertDecayScale =
		night.alertDecayScale * nightWeight + day.alertDecayScale * dayWeight;
	return sample;
}

float slotValue(int pokemonId)
{
	std::uint32_t value = static_cast<std::uint32_t>(pokemonId + 1);
	value ^= value >> 16;
	value *= 0x7feb352du;
	value ^= value >> 15;
	value *= 0x846ca68bu;
	value ^= value >> 16;
	return static_cast<float>(value & 0x00ffffffu) / 16777215.0f;
}
}

PokemonEcologySample samplePokemonEcology(PokemonSpecies species,
	                                       float daylight)
{
	PokemonEcologySample night;
	PokemonEcologySample day;
	switch (species)
	{
	case PokemonSpecies::Umbreon:
		night = {1.15f, 1.25f, 1.20f, 1.25f, 0.75f};
		day = {0.75f, 0.82f, 0.85f, 0.72f, 1.15f};
		break;
	case PokemonSpecies::Bulbasaur:
		night = {0.78f, 0.88f, 0.90f, 0.82f, 1.12f};
		day = {1.08f, 1.04f, 1.02f, 1.00f, 1.00f};
		break;
	case PokemonSpecies::Eevee:
		night = {0.70f, 0.82f, 0.85f, 0.75f, 1.20f};
		day = {1.12f, 1.05f, 1.05f, 1.00f, 1.00f};
		break;
	case PokemonSpecies::Charizard:
		break;
	}
	return mixSamples(night, day, daylight);
}

float pokemonEcologyPresenceFraction(PokemonSpecies species, float daylight)
{
	const float dayWeight = std::max(0.0f, std::min(1.0f, daylight));
	switch (species)
	{
	case PokemonSpecies::Umbreon:
		return 1.0f - dayWeight * 0.70f;
	case PokemonSpecies::Bulbasaur:
		return 0.35f + dayWeight * 0.65f;
	case PokemonSpecies::Eevee:
		return 0.50f + dayWeight * 0.50f;
	case PokemonSpecies::Charizard:
		return 1.0f;
	}
	return 1.0f;
}

bool pokemonEcologySlotPresent(PokemonSpecies species, int pokemonId,
	                           float daylight)
{
	if (pokemonId < 0 || pokemonId == 0)
	{
		return true;
	}
	return slotValue(pokemonId) <=
	       pokemonEcologyPresenceFraction(species, daylight);
}

const char *pokemonEcologyPhaseName(float daylight)
{
	if (daylight <= 0.25f)
	{
		return "Night";
	}
	if (daylight >= 0.70f)
	{
		return "Day";
	}
	return "Twilight";
}

const char *pokemonEcologyFieldHint(float daylight)
{
	if (daylight <= 0.25f)
	{
		return "Umbreon emerging · meadow wildlife sheltering";
	}
	if (daylight >= 0.70f)
	{
		return "Eevee and Bulbasaur active · Umbreon sheltering";
	}
	return "Wildlife activity shifting";
}
