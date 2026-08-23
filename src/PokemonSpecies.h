#pragma once

enum class PokemonSpecies
{
	Umbreon,
	Bulbasaur,
	Charizard,
};

PokemonSpecies groundPokemonSpeciesForIndex(int index);
const char *pokemonSpeciesName(PokemonSpecies species);
