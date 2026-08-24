#pragma once

#include <vector>

#include <glm/glm.hpp>

struct FieldRadarCandidate
{
	int id = -1;
	bool available = true;
	glm::vec3 position = glm::vec3(0.0f);
};

struct FieldRadarContact
{
	int id = -1;
	float distance = 0.0f;
	float bearingRadians = 0.0f;

	bool valid() const;
};

FieldRadarContact selectNearestFieldRadarContact(
	const glm::vec3 &playerPosition, float playerYaw,
	const std::vector<FieldRadarCandidate> &candidates);
