#pragma once

#include <vector>

#include <glm/glm.hpp>

struct PokemonNavigationBlocker
{
	int id = -1;
	glm::vec2 center = glm::vec2(0.0f);
	float radius = 0.0f;
};

struct PokemonNavigationResult
{
	glm::vec2 position = glm::vec2(0.0f);
	glm::vec2 collisionNormal = glm::vec2(0.0f);
	bool collided = false;
};

glm::vec3 steerGroundPokemonVelocity(
	const glm::vec3 &position, const glm::vec3 &desiredVelocity,
	float selfRadius, int selfId,
	const std::vector<PokemonNavigationBlocker> &blockers);

PokemonNavigationResult resolveGroundPokemonPosition(
	const glm::vec2 &proposedPosition,
	float selfRadius, int selfId,
	const std::vector<PokemonNavigationBlocker> &blockers);
