#pragma once

#include <vector>

#include <glm/glm.hpp>

constexpr float GROUND_TARGETING_RANGE = 14.0f;
constexpr float FLYING_TARGETING_RANGE = 24.0f;

struct PokemonTargetCandidate
{
	int index = -1;
	bool flying = false;
	bool caught = false;
	glm::vec3 position = glm::vec3(0.0f);
};

struct PokemonTargetSelection
{
	int index = -1;
	bool flying = false;
	float distance = 0.0f;
	float alignment = 0.0f;

	bool valid() const;
};

struct PokemonTargetingConfig
{
	float groundRange = GROUND_TARGETING_RANGE;
	float flyingRange = FLYING_TARGETING_RANGE;
	float minimumAlignment = 0.15f;
	float alignmentPenalty = 8.0f;
};

PokemonTargetSelection selectPokemonTarget(
	const glm::vec3 &playerPosition,
	float playerYaw,
	const std::vector<PokemonTargetCandidate> &candidates,
	const PokemonTargetingConfig &config = PokemonTargetingConfig());
