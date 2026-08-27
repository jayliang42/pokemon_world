#include "PokemonSpawn.h"

const PokemonSpawnArea &pokemonSpawnArea(PokemonSpecies species)
{
	static const PokemonSpawnArea umbreon = {
		WorldRegionKind::MoonshadowEdge, {-22.0f, -21.0f}, 10.0f, 18.0f};
	static const PokemonSpawnArea bulbasaur = {
		WorldRegionKind::WindwhisperMeadow, {0.0f, 0.0f}, 10.0f, 30.0f};
	static const PokemonSpawnArea eevee = {
		WorldRegionKind::WindwhisperMeadow, {0.0f, 8.0f}, 9.0f, 27.0f};
	static const PokemonSpawnArea charizard = {
		WorldRegionKind::RedrockHighlands, {20.0f, -12.0f}, 10.0f, 19.0f};

	switch (species)
	{
	case PokemonSpecies::Umbreon:
		return umbreon;
	case PokemonSpecies::Bulbasaur:
		return bulbasaur;
	case PokemonSpecies::Eevee:
		return eevee;
	case PokemonSpecies::Charizard:
		return charizard;
	}
	return bulbasaur;
}
