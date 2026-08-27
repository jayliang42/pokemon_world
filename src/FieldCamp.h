#pragma once

#include <glm/glm.hpp>

struct FieldCampLayout
{
	glm::vec2 center = glm::vec2(0.0f);
	glm::vec2 tentCenter = glm::vec2(-4.0f, 1.5f);
	glm::vec2 workbenchCenter = glm::vec2(3.5f, -1.5f);
	glm::vec2 supplyCrateCenter = glm::vec2(3.5f, 1.5f);
	glm::vec2 spawnPosition = glm::vec2(0.0f, 6.0f);
	float spawnYaw = 0.0f;
	float interactionRadius = 6.5f;
	float wildExclusionRadius = 7.2f;
	float landingRadius = 2.6f;
};

FieldCampLayout defaultFieldCampLayout();
float horizontalDistanceToCamp(const glm::vec3 &position,
	                           const FieldCampLayout &layout);
bool isInsideCampInteractionRange(const glm::vec3 &position,
	                              const FieldCampLayout &layout);
