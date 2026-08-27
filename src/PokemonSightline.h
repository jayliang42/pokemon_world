#pragma once

#include <functional>
#include <vector>

#include <glm/glm.hpp>

struct PokemonSightlineCylinder
{
	glm::vec2 center = glm::vec2(0.0f);
	float radius = 1.0f;
	float baseY = 0.0f;
	float height = 1.0f;
};

using PokemonSightlineHeightProvider = std::function<float(float, float)>;

bool pokemonSightlineClear(
	const glm::vec3 &observerEye, const glm::vec3 &subjectCenter,
	const std::vector<PokemonSightlineCylinder> &cylinders,
	const PokemonSightlineHeightProvider &groundHeightProvider);
