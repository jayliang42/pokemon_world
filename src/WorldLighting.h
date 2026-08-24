#pragma once

#include <glm/glm.hpp>

constexpr float WORLD_LIGHTING_CYCLE_SECONDS = 300.0f;

struct WorldLighting
{
	glm::vec3 sunDirection = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 sunColor = glm::vec3(1.0f);
	glm::vec3 ambientColor = glm::vec3(0.5f);
	glm::vec3 fogColor = glm::vec3(0.7f);
	glm::vec3 skyHorizonColor = glm::vec3(0.7f);
	glm::vec3 skyZenithColor = glm::vec3(0.2f);
	float fogStart = 26.0f;
	float fogEnd = 62.0f;
	float daylight = 1.0f;
};

float worldLightingCyclePhase(double elapsedSeconds);
WorldLighting sampleWorldLighting(float cyclePhase);
