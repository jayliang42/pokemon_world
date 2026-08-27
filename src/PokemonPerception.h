#pragma once

#include <glm/glm.hpp>

struct PokemonPerceptionConfig
{
	float visionRange = 12.0f;
	float visionHalfAngleRadians = 1.0471976f;
	float verticalTolerance = 8.0f;
	float hearingRange = 7.0f;
	float visionAlertGainPerSecond = 0.85f;
	float hearingAlertGainPerSecond = 0.45f;
	float alertDecayPerSecond = 0.20f;
};

struct PokemonPerceptionInput
{
	glm::vec3 observerPosition = glm::vec3(0.0f);
	float observerHeading = 0.0f;
	glm::vec3 subjectPosition = glm::vec3(0.0f);
	float subjectNoise = 0.0f;
	float currentAlertness = 0.0f;
	float deltaSeconds = 0.0f;
	bool lineOfSightClear = true;
};

struct PokemonPerceptionSample
{
	bool visible = false;
	bool heard = false;
	float distance = 0.0f;
	float facingAlignment = -1.0f;
	float alertness = 0.0f;
};

PokemonPerceptionSample samplePokemonPerception(
	const PokemonPerceptionConfig &config,
	const PokemonPerceptionInput &input);
