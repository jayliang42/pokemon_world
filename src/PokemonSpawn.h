#pragma once

#include <glm/glm.hpp>

#include "PokemonSpecies.h"
#include "WorldRegion.h"

struct PokemonSpawnArea
{
	WorldRegionKind region = WorldRegionKind::WindwhisperMeadow;
	glm::vec2 center = glm::vec2(0.0f);
	float minimumRadius = 0.0f;
	float maximumRadius = 1.0f;
};

const PokemonSpawnArea &pokemonSpawnArea(PokemonSpecies species);
