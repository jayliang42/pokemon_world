#include "FieldRadar.h"

#include <cmath>
#include <limits>

namespace
{
constexpr float PI = 3.14159265358979323846f;

float wrapAngle(float radians)
{
	while (radians > PI)
	{
		radians -= 2.0f * PI;
	}
	while (radians < -PI)
	{
		radians += 2.0f * PI;
	}
	return radians;
}
}

bool FieldRadarContact::valid() const
{
	return id >= 0;
}

FieldRadarContact selectNearestFieldRadarContact(
	const glm::vec3 &playerPosition, float playerYaw,
	const std::vector<FieldRadarCandidate> &candidates)
{
	FieldRadarContact contact;
	float bestDistance = std::numeric_limits<float>::max();
	for (const FieldRadarCandidate &candidate : candidates)
	{
		if (!candidate.available || candidate.id < 0)
		{
			continue;
		}

		const glm::vec2 offset(candidate.position.x - playerPosition.x,
		                       candidate.position.z - playerPosition.z);
		const float distance = glm::length(offset);
		if (distance > bestDistance ||
		    (distance == bestDistance && contact.valid() && candidate.id > contact.id))
		{
			continue;
		}

		bestDistance = distance;
		contact.id = candidate.id;
		contact.distance = distance;
		contact.bearingRadians =
			wrapAngle(std::atan2(offset.x, -offset.y) + playerYaw);
	}
	return contact;
}
