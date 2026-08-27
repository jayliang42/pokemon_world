#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "PokemonSpecies.h"

constexpr float POKEMON_GROUP_ALERT_RADIUS = 11.0f;
constexpr float POKEMON_GROUP_ALERT_MAX_RECIPIENT_ALERTNESS = 0.60f;
constexpr double POKEMON_GROUP_ALERT_COOLDOWN_SECONDS = 2.5;

struct PokemonGroupAlertCandidate
{
	int id = -1;
	PokemonSpecies species = PokemonSpecies::Umbreon;
	glm::vec3 position = glm::vec3(0.0f);
	float alertness = 0.0f;
	bool eligible = false;
	bool sightlineClear = false;
};

struct PokemonGroupAlertState
{
	double nextPropagationTime = 0.0;
};

struct PokemonGroupAlertResult
{
	std::vector<int> recipientIds;
	bool propagated = false;
};

PokemonGroupAlertResult propagatePokemonGroupAlert(
	PokemonGroupAlertState &state, double now, int sourceId,
	PokemonSpecies sourceSpecies, const glm::vec3 &sourcePosition,
	const std::vector<PokemonGroupAlertCandidate> &candidates);
