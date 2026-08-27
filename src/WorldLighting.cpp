#include "WorldLighting.h"

#include <algorithm>
#include <cmath>

namespace
{
float wrapUnit(float value)
{
	const float wrapped = std::fmod(value, 1.0f);
	return wrapped < 0.0f ? wrapped + 1.0f : wrapped;
}
}

float worldLightingCyclePhase(double elapsedSeconds)
{
	const float elapsedPhase = static_cast<float>(elapsedSeconds /
	                                               WORLD_LIGHTING_CYCLE_SECONDS);
	return wrapUnit(0.25f + elapsedPhase);
}

WorldLighting sampleWorldLighting(float cyclePhase)
{
	constexpr float PI = 3.14159265358979323846f;
	const float phase = wrapUnit(cyclePhase);
	const float elevation = std::sin(phase * 2.0f * PI) * 0.82f;
	const float horizontal = std::sqrt(std::max(0.0f, 1.0f - elevation * elevation));
	const float azimuth = -2.406f + (phase - 0.25f) * 2.0f * PI * 0.38f;
	const float daylight = glm::smoothstep(-0.09f, 0.16f, elevation);

	WorldLighting lighting;
	lighting.sunDirection = glm::normalize(glm::vec3(
		std::sin(azimuth) * horizontal, elevation,
		std::cos(azimuth) * horizontal));
	lighting.daylight = daylight;
	lighting.sunColor = glm::mix(glm::vec3(0.22f, 0.29f, 0.48f),
	                             glm::vec3(1.0f, 0.91f, 0.74f), daylight);
	lighting.ambientColor = glm::mix(glm::vec3(0.32f, 0.38f, 0.52f),
	                                 glm::vec3(0.52f, 0.60f, 0.70f), daylight);
	lighting.fogColor = glm::mix(glm::vec3(0.10f, 0.16f, 0.31f),
	                             glm::vec3(0.66f, 0.84f, 0.96f), daylight);
	lighting.skyHorizonColor = glm::mix(glm::vec3(0.11f, 0.19f, 0.36f),
	                                    glm::vec3(0.74f, 0.91f, 1.0f), daylight);
	lighting.skyZenithColor = glm::mix(glm::vec3(0.035f, 0.085f, 0.23f),
	                                   glm::vec3(0.12f, 0.48f, 0.94f), daylight);
	lighting.fogStart = glm::mix(18.0f, 26.0f, daylight);
	lighting.fogEnd = glm::mix(48.0f, 62.0f, daylight);
	return lighting;
}
