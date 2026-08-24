#include "PokemonNavigation.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr float NAVIGATION_CLEARANCE = 0.10f;
constexpr float STEERING_LOOK_AHEAD = 1.80f;
constexpr float STEERING_RADIUS = 0.80f;
constexpr float MINIMUM_VECTOR_LENGTH = 0.0001f;

float clampValue(float value, float minimum, float maximum)
{
	return std::max(minimum, std::min(maximum, value));
}

bool isBlocking(const PokemonNavigationBlocker &blocker, int selfId)
{
	return blocker.id != selfId && blocker.radius > 0.0f;
}

glm::vec2 fallbackNormal(int selfId, int blockerId)
{
	const unsigned int hash = static_cast<unsigned int>(selfId * 1103515245u) ^
	                          static_cast<unsigned int>(blockerId * 12345u);
	const float angle = static_cast<float>(hash % 360u) *
	                    0.01745329251994329577f;
	return glm::vec2(std::cos(angle), std::sin(angle));
}

glm::vec2 collisionNormal(const glm::vec2 &difference, int selfId,
	                        int blockerId)
{
	const float length = glm::length(difference);
	return length > MINIMUM_VECTOR_LENGTH
	           ? difference / length
	           : fallbackNormal(selfId, blockerId);
}
}

glm::vec3 steerGroundPokemonVelocity(
	const glm::vec3 &position, const glm::vec3 &desiredVelocity,
	float selfRadius, int selfId,
	const std::vector<PokemonNavigationBlocker> &blockers)
{
	const glm::vec2 desiredPlanar(desiredVelocity.x, desiredVelocity.z);
	const float speed = glm::length(desiredPlanar);
	if (speed <= MINIMUM_VECTOR_LENGTH)
	{
		return desiredVelocity;
	}

	const glm::vec2 direction = desiredPlanar / speed;
	const glm::vec2 planarPosition(position.x, position.z);
	glm::vec2 steering(0.0f);
	for (const PokemonNavigationBlocker &blocker : blockers)
	{
		if (!isBlocking(blocker, selfId))
		{
			continue;
		}

		const float minimumDistance =
			std::max(0.0f, selfRadius) + blocker.radius + NAVIGATION_CLEARANCE;
		const float lookAhead = minimumDistance + STEERING_LOOK_AHEAD;
		const glm::vec2 toBlocker = blocker.center - planarPosition;
		const float forwardDistance = glm::dot(toBlocker, direction);
		if (forwardDistance < -minimumDistance || forwardDistance > lookAhead)
		{
			continue;
		}

		const float closestDistanceAlongPath = clampValue(
			forwardDistance, 0.0f, lookAhead);
		const glm::vec2 closestPoint =
			planarPosition + direction * closestDistanceAlongPath;
		const glm::vec2 away = closestPoint - blocker.center;
		const float separation = glm::length(away);
		const float influenceDistance = minimumDistance + STEERING_RADIUS;
		if (separation >= influenceDistance)
		{
			continue;
		}

		const glm::vec2 normal = collisionNormal(away, selfId, blocker.id);
		const glm::vec2 left(-direction.y, direction.x);
		const float cross = direction.x * toBlocker.y - direction.y * toBlocker.x;
		const float side = std::fabs(cross) > MINIMUM_VECTOR_LENGTH
		                       ? (cross > 0.0f ? -1.0f : 1.0f)
		                       : (fallbackNormal(selfId, blocker.id).x >= 0.0f
		                              ? 1.0f
		                              : -1.0f);
		const glm::vec2 turnDirection = glm::normalize(
			normal * 0.28f + left * side * 0.92f);
		const float urgency = 1.0f - clampValue(
			(separation - minimumDistance) / STEERING_RADIUS, 0.0f, 1.0f);
		steering += turnDirection * urgency;
	}

	if (glm::length(steering) <= MINIMUM_VECTOR_LENGTH)
	{
		return desiredVelocity;
	}
	const glm::vec2 steeredDirection = glm::normalize(direction + steering);
	return glm::vec3(steeredDirection.x * speed, desiredVelocity.y,
	                 steeredDirection.y * speed);
}

PokemonNavigationResult resolveGroundPokemonPosition(
	const glm::vec2 &proposedPosition,
	float selfRadius, int selfId,
	const std::vector<PokemonNavigationBlocker> &blockers)
{
	PokemonNavigationResult result;
	result.position = proposedPosition;
	for (int pass = 0; pass < 4; ++pass)
	{
		bool resolvedThisPass = false;
		for (const PokemonNavigationBlocker &blocker : blockers)
		{
			if (!isBlocking(blocker, selfId))
			{
				continue;
			}

			const float minimumDistance = std::max(0.0f, selfRadius) +
			                              blocker.radius + NAVIGATION_CLEARANCE;
			const glm::vec2 difference = result.position - blocker.center;
			if (glm::length(difference) >= minimumDistance)
			{
				continue;
			}

			result.collisionNormal = collisionNormal(difference, selfId, blocker.id);
			result.position = blocker.center +
			                  result.collisionNormal * minimumDistance;
			result.collided = true;
			resolvedThisPass = true;
		}
		if (!resolvedThisPass)
		{
			break;
		}
	}
	return result;
}
