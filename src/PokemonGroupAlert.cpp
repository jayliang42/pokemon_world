#include "PokemonGroupAlert.h"

#include <cmath>

namespace
{
bool finitePosition(const glm::vec3 &position)
{
	return std::isfinite(position.x) && std::isfinite(position.y) &&
	       std::isfinite(position.z);
}
}

PokemonGroupAlertResult propagatePokemonGroupAlert(
	PokemonGroupAlertState &state, double now, int sourceId,
	PokemonSpecies sourceSpecies, const glm::vec3 &sourcePosition,
	const std::vector<PokemonGroupAlertCandidate> &candidates)
{
	PokemonGroupAlertResult result;
	if (!std::isfinite(now) || now < 0.0 || sourceId < 0 ||
	    !finitePosition(sourcePosition) ||
	    !std::isfinite(state.nextPropagationTime) ||
	    now < state.nextPropagationTime)
	{
		return result;
	}
	for (const PokemonGroupAlertCandidate &candidate : candidates)
	{
		if (!candidate.eligible || !candidate.sightlineClear ||
		    candidate.id < 0 || candidate.id == sourceId ||
		    candidate.species != sourceSpecies ||
		    !finitePosition(candidate.position) ||
		    !std::isfinite(candidate.alertness) || candidate.alertness < 0.0f ||
		    candidate.alertness >=
		        POKEMON_GROUP_ALERT_MAX_RECIPIENT_ALERTNESS)
		{
			continue;
		}
		const glm::vec2 offset(candidate.position.x - sourcePosition.x,
		                       candidate.position.z - sourcePosition.z);
		if (glm::length(offset) <= POKEMON_GROUP_ALERT_RADIUS)
		{
			result.recipientIds.push_back(candidate.id);
		}
	}
	if (!result.recipientIds.empty())
	{
		result.propagated = true;
		state.nextPropagationTime =
			now + POKEMON_GROUP_ALERT_COOLDOWN_SECONDS;
	}
	return result;
}
