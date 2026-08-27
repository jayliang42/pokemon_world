#include "CaptureMechanics.h"

#include <algorithm>
#include <cmath>

namespace
{
float clampValue(float value, float minimum, float maximum)
{
	return std::max(minimum, std::min(maximum, value));
}

float speciesBaseProbability(PokemonSpecies species)
{
	switch (species)
	{
	case PokemonSpecies::Bulbasaur:
		return 0.72f;
	case PokemonSpecies::Eevee:
		return 0.64f;
	case PokemonSpecies::Umbreon:
		return 0.56f;
	case PokemonSpecies::Charizard:
		return 0.38f;
	}
	return 0.50f;
}

float activityMultiplier(CaptureActivity activity)
{
	switch (activity)
	{
	case CaptureActivity::Idle:
		return 1.08f;
	case CaptureActivity::Moving:
		return 0.96f;
	case CaptureActivity::Fleeing:
		return 0.72f;
	}
	return 1.0f;
}
}

float calculateCaptureProbability(const CaptureAttempt &attempt)
{
	const float safeMaximumDistance = std::max(attempt.maximumDistance, 0.001f);
	const float distanceRatio =
		clampValue(attempt.distance / safeMaximumDistance, 0.0f, 1.0f);
	const float distanceMultiplier = 1.15f - distanceRatio * 0.45f;
	const float alignment = clampValue(attempt.alignment, 0.0f, 1.0f);
	const float alignmentMultiplier = 0.82f + alignment * 0.18f;
	const float healthRatio = clampValue(attempt.healthRatio, 0.0f, 1.0f);
	const float healthMultiplier = 0.85f + (1.0f - healthRatio) * 0.50f;
	const float alertness = clampValue(attempt.alertness, 0.0f, 1.0f);
	const float alertnessMultiplier = 1.08f - alertness * 0.38f;
	const float backHitMultiplier = attempt.backHit ? 1.18f : 1.0f;
	const float lureMultiplier = attempt.lured ? 1.14f : 1.0f;
	const float difficultyMultiplier =
		clampValue(attempt.difficultyMultiplier, 0.10f, 1.0f);
	const float probability = speciesBaseProbability(attempt.species) *
	                          distanceMultiplier * alignmentMultiplier *
	                          activityMultiplier(attempt.activity) *
	                          healthMultiplier * alertnessMultiplier *
	                          backHitMultiplier * lureMultiplier *
	                          difficultyMultiplier;
	return clampValue(probability, 0.12f, 0.95f);
}

bool isCaptureBackHit(float targetHeading,
	                  float targetToThrowerX,
	                  float targetToThrowerZ)
{
	const float directionLength = std::sqrt(
		targetToThrowerX * targetToThrowerX +
		targetToThrowerZ * targetToThrowerZ);
	if (directionLength <= 0.0001f)
	{
		return false;
	}
	const float forwardX = std::sin(targetHeading);
	const float forwardZ = std::cos(targetHeading);
	const float facingDot =
		forwardX * targetToThrowerX / directionLength +
		forwardZ * targetToThrowerZ / directionLength;
	return facingDot <= -0.35f;
}

CaptureResult resolveCaptureAttempt(const CaptureAttempt &attempt,
	                                float randomRoll)
{
	CaptureResult result;
	result.probability = calculateCaptureProbability(attempt);
	const float roll = clampValue(randomRoll, 0.0f, 1.0f);
	result.captured = roll < result.probability;
	if (result.captured)
	{
		result.shakes = 3;
		return result;
	}

	const float missRange = std::max(1.0f - result.probability, 0.001f);
	const float missPosition = (roll - result.probability) / missRange;
	if (missPosition < 0.33f)
	{
		result.shakes = 2;
	}
	else if (missPosition < 0.72f)
	{
		result.shakes = 1;
	}
	return result;
}

CaptureRandom::CaptureRandom(std::uint32_t seed)
	: state_(seed != 0u ? seed : 0xC0FFEEu)
{
}

float CaptureRandom::nextUnit()
{
	state_ = state_ * 1664525u + 1013904223u;
	return static_cast<float>(state_ >> 8) / 16777216.0f;
}
