#pragma once

#include <glm/glm.hpp>

#include "PokemonSpecies.h"

constexpr float FIELD_LURE_DURATION_SECONDS = 14.0f;
constexpr float FIELD_LURE_ATTRACTION_RADIUS = 18.0f;
constexpr float FIELD_LURE_CAPTURE_BONUS_RADIUS = 3.5f;
constexpr float FIELD_LURE_MAX_ATTRACTION_ALERTNESS = 0.35f;

struct FieldLureState
{
	bool active = false;
	glm::vec3 position = glm::vec3(0.0f);
	float remainingSeconds = 0.0f;
};

enum class FieldLureDeployStatus
{
	Deployed,
	Locked,
	Empty,
	Airborne,
	Unavailable,
	AlreadyActive,
};

struct FieldLureDeployResult
{
	FieldLureDeployStatus status = FieldLureDeployStatus::Unavailable;
	int remainingInventory = 0;
	FieldLureState lure;
};

FieldLureDeployResult deployFieldLure(
	bool unlocked, int inventory, bool grounded, bool gameplayAvailable,
	const glm::vec3 &position, bool lureAlreadyActive = false);
bool updateFieldLure(FieldLureState &lure, float deltaSeconds);
bool fieldLureAttracts(PokemonSpecies species,
	                   const glm::vec3 &pokemonPosition, float alertness,
	                   const FieldLureState &lure);
bool fieldLureCaptureBonusApplies(const glm::vec3 &pokemonPosition,
	                              const FieldLureState &lure);
