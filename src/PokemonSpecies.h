#pragma once

enum class PokemonSpecies
{
	Umbreon,
	Bulbasaur,
	Eevee,
	Charizard,
};

PokemonSpecies groundPokemonSpeciesForIndex(int index);
const char *pokemonSpeciesName(PokemonSpecies species);
