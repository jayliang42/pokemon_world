#include "FieldLure.h"

#include <algorithm>
#include <cmath>

namespace
{
float horizontalDistance(const glm::vec3 &first, const glm::vec3 &second)
{
	return glm::length(glm::vec2(first.x - second.x, first.z - second.z));
}
}

FieldLureDeployResult deployFieldLure(
	bool unlocked, int inventory, bool grounded, bool gameplayAvailable,
	const glm::vec3 &position, bool lureAlreadyActive)
{
	FieldLureDeployResult result;
	result.remainingInventory = std::max(0, inventory);
	if (!unlocked)
	{
		result.status = FieldLureDeployStatus::Locked;
		return result;
	}
	if (result.remainingInventory <= 0)
	{
		result.status = FieldLureDeployStatus::Empty;
		return result;
	}
	if (!grounded)
	{
		result.status = FieldLureDeployStatus::Airborne;
		return result;
	}
	if (!gameplayAvailable)
	{
		result.status = FieldLureDeployStatus::Unavailable;
		return result;
	}
	if (lureAlreadyActive)
	{
		result.status = FieldLureDeployStatus::AlreadyActive;
		return result;
	}

	result.status = FieldLureDeployStatus::Deployed;
	--result.remainingInventory;
	result.lure.active = true;
	result.lure.position = position;
	result.lure.remainingSeconds = FIELD_LURE_DURATION_SECONDS;
	return result;
}

bool updateFieldLure(FieldLureState &lure, float deltaSeconds)
{
	if (!lure.active || !std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f)
	{
		return false;
	}
	lure.remainingSeconds =
		std::max(0.0f, lure.remainingSeconds - deltaSeconds);
	if (lure.remainingSeconds > 0.0f)
	{
		return false;
	}
	lure.active = false;
	return true;
}

bool fieldLureAttracts(PokemonSpecies species,
	                   const glm::vec3 &pokemonPosition, float alertness,
	                   const FieldLureState &lure)
{
	return lure.active && species == PokemonSpecies::Eevee &&
	       alertness >= 0.0f &&
	       alertness < FIELD_LURE_MAX_ATTRACTION_ALERTNESS &&
	       horizontalDistance(pokemonPosition, lure.position) <=
	           FIELD_LURE_ATTRACTION_RADIUS;
}

bool fieldLureCaptureBonusApplies(const glm::vec3 &pokemonPosition,
	                              const FieldLureState &lure)
{
	return lure.active &&
	       horizontalDistance(pokemonPosition, lure.position) <=
	           FIELD_LURE_CAPTURE_BONUS_RADIUS;
}
