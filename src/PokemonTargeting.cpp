#include "PokemonTargeting.h"

#include <cmath>
#include <limits>

bool PokemonTargetSelection::valid() const
{
	return index >= 0;
}

PokemonTargetSelection selectPokemonTarget(
	const glm::vec3 &playerPosition,
	float playerYaw,
	const std::vector<PokemonTargetCandidate> &candidates,
	const PokemonTargetingConfig &config)
{
	PokemonTargetSelection selection;
	const glm::vec2 forward(-std::sin(playerYaw), -std::cos(playerYaw));
	float bestScore = std::numeric_limits<float>::max();

	for (const PokemonTargetCandidate &candidate : candidates)
	{
		if (candidate.caught || candidate.index < 0)
		{
			continue;
		}

		const glm::vec3 offset = candidate.position - playerPosition;
		const float distance = glm::length(offset);
		const float maximumRange = candidate.flying ? config.flyingRange : config.groundRange;
		if (distance > maximumRange)
		{
			continue;
		}

		const glm::vec2 horizontalOffset(offset.x, offset.z);
		const float horizontalLength = glm::length(horizontalOffset);
		const float alignment = horizontalLength <= 0.0001f
		                            ? 1.0f
		                            : glm::dot(horizontalOffset / horizontalLength, forward);
		if (alignment < config.minimumAlignment)
		{
			continue;
		}

		const float score = distance + (1.0f - alignment) * config.alignmentPenalty;
		if (score >= bestScore)
		{
			continue;
		}

		bestScore = score;
		selection.index = candidate.index;
		selection.flying = candidate.flying;
		selection.distance = distance;
		selection.alignment = alignment;
	}

	return selection;
}
