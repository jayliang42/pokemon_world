#include "PokemonSpecies.h"

PokemonSpecies groundPokemonSpeciesForIndex(int index)
{
	if (index % 6 == 3)
	{
		return PokemonSpecies::Eevee;
	}
	return index % 2 == 0
	           ? PokemonSpecies::Umbreon
	           : PokemonSpecies::Bulbasaur;
}

const char *pokemonSpeciesName(PokemonSpecies species)
{
	switch (species)
	{
	case PokemonSpecies::Umbreon:
		return "Umbreon";
	case PokemonSpecies::Bulbasaur:
		return "Bulbasaur";
	case PokemonSpecies::Eevee:
		return "Eevee";
	case PokemonSpecies::Charizard:
		return "Charizard";
	}
	return "Pokemon";
}
