#include "PokemonPerception.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float MINIMUM_DIRECTION_LENGTH = 0.0001f;

float clampValue(float value, float minimum, float maximum)
{
	return std::max(minimum, std::min(maximum, value));
}
}

PokemonPerceptionSample samplePokemonPerception(
	const PokemonPerceptionConfig &config,
	const PokemonPerceptionInput &input)
{
	PokemonPerceptionSample sample;
	const glm::vec2 offset(input.subjectPosition.x - input.observerPosition.x,
	                       input.subjectPosition.z - input.observerPosition.z);
	sample.distance = glm::length(offset);
	const float verticalDistance =
		std::fabs(input.subjectPosition.y - input.observerPosition.y);

	if (sample.distance <= MINIMUM_DIRECTION_LENGTH)
	{
		sample.facingAlignment = 1.0f;
	}
	else
	{
		const glm::vec2 forward(std::sin(input.observerHeading),
		                        std::cos(input.observerHeading));
		sample.facingAlignment = glm::dot(forward, offset / sample.distance);
	}

	const float visionRange = std::max(config.visionRange, 0.0f);
	const float halfAngle = clampValue(
		config.visionHalfAngleRadians, 0.0f, 3.14159265f);
	const float coneThreshold = std::cos(halfAngle);
	sample.visible = input.lineOfSightClear && visionRange > 0.0f &&
	                 sample.distance <= visionRange &&
	                 verticalDistance <= std::max(config.verticalTolerance, 0.0f) &&
	                 sample.facingAlignment >= coneThreshold;

	const float noise = clampValue(input.subjectNoise, 0.0f, 1.0f);
	const float effectiveHearingRange =
		std::max(config.hearingRange, 0.0f) * std::sqrt(noise);
	sample.heard = effectiveHearingRange > 0.0f &&
	               glm::distance(input.observerPosition, input.subjectPosition) <=
	                   effectiveHearingRange;

	const float currentAlertness =
		clampValue(input.currentAlertness, 0.0f, 1.0f);
	const float deltaSeconds = std::max(input.deltaSeconds, 0.0f);
	float alertGain = 0.0f;
	if (sample.visible)
	{
		const float distanceStrength =
			1.0f - clampValue(sample.distance / visionRange, 0.0f, 1.0f);
		const float angleDenominator = std::max(1.0f - coneThreshold, 0.001f);
		const float angleStrength = clampValue(
			(sample.facingAlignment - coneThreshold) / angleDenominator,
			0.0f, 1.0f);
		const float visionStrength =
			0.35f + distanceStrength * 0.40f + angleStrength * 0.25f;
		alertGain = std::max(
			alertGain,
			std::max(config.visionAlertGainPerSecond, 0.0f) * visionStrength);
	}
	if (sample.heard)
	{
		const float hearingStrength = effectiveHearingRange > 0.0f
			? 1.0f - clampValue(sample.distance / effectiveHearingRange,
			                    0.0f, 1.0f) * 0.5f
			: 0.0f;
		alertGain = std::max(
			alertGain,
			std::max(config.hearingAlertGainPerSecond, 0.0f) * noise *
				hearingStrength);
	}

	if (sample.visible || sample.heard)
	{
		sample.alertness = currentAlertness + alertGain * deltaSeconds;
	}
	else
	{
		sample.alertness = currentAlertness -
		                    std::max(config.alertDecayPerSecond, 0.0f) *
		                        deltaSeconds;
	}
	sample.alertness = clampValue(sample.alertness, 0.0f, 1.0f);
	return sample;
}
