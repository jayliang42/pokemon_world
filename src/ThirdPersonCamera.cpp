#include "ThirdPersonCamera.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
constexpr float CLEAR_FRACTION_EPSILON = 0.01f;
constexpr int COLLISION_REFINEMENT_STEPS = 8;
}

ThirdPersonCamera::ThirdPersonCamera(const ThirdPersonCameraConfig &config)
	: config_(config)
{
}

ThirdPersonCameraPose ThirdPersonCamera::update(const glm::vec3 &playerPosition,
	                                             float playerYaw,
	                                             float deltaSeconds)
{
	ThirdPersonCameraPose pose;
	pose.target = playerPosition + glm::vec3(0.0f, config_.targetHeight, 0.0f);
	const glm::vec3 forward(-std::sin(playerYaw), 0.0f, -std::cos(playerYaw));
	const glm::vec3 desiredPosition = pose.target - forward * config_.boomDistance +
	                                  glm::vec3(0.0f, config_.boomHeight, 0.0f);
	const float clearFraction = maximumClearBoomFraction(pose.target, desiredPosition);
	pose.obstructed = clearFraction < 1.0f - CLEAR_FRACTION_EPSILON;

	deltaSeconds = std::max(0.0f, std::min(0.1f, deltaSeconds));
	if (clearFraction < currentBoomFraction_)
	{
		currentBoomFraction_ = clearFraction;
	}
	else
	{
		currentBoomFraction_ = std::min(clearFraction,
		                               currentBoomFraction_ + config_.returnSpeed * deltaSeconds);
	}

	pose.boomFraction = currentBoomFraction_;
	pose.position = pose.target + (desiredPosition - pose.target) * currentBoomFraction_;
	return pose;
}

void ThirdPersonCamera::reset()
{
	currentBoomFraction_ = 1.0f;
}

void ThirdPersonCamera::setGroundHeightProvider(GroundHeightProvider provider)
{
	groundHeightProvider_ = std::move(provider);
}

void ThirdPersonCamera::setStaticObstacles(std::vector<StaticCollisionCylinder> obstacles)
{
	staticObstacles_ = std::move(obstacles);
}

float ThirdPersonCamera::maximumClearBoomFraction(const glm::vec3 &target,
	                                                const glm::vec3 &desiredPosition) const
{
	const glm::vec3 boom = desiredPosition - target;
	const float boomLength = glm::length(boom);
	if (boomLength <= 0.0001f)
	{
		return 1.0f;
	}

	const float minimumFraction = std::min(1.0f, config_.minimumDistance / boomLength);
	const int sampleCount = std::max(2, config_.collisionSamples);
	float previousFraction = 0.0f;
	for (int sample = 1; sample <= sampleCount; ++sample)
	{
		const float fraction = static_cast<float>(sample) / static_cast<float>(sampleCount);
		if (!collidesAt(target + boom * fraction))
		{
			previousFraction = fraction;
			continue;
		}

		float clearFraction = previousFraction;
		float blockedFraction = fraction;
		for (int refinement = 0; refinement < COLLISION_REFINEMENT_STEPS; ++refinement)
		{
			const float midpoint = (clearFraction + blockedFraction) * 0.5f;
			if (collidesAt(target + boom * midpoint))
			{
				blockedFraction = midpoint;
			}
			else
			{
				clearFraction = midpoint;
			}
		}
		return std::max(minimumFraction, clearFraction - CLEAR_FRACTION_EPSILON);
	}

	return 1.0f;
}

bool ThirdPersonCamera::collidesAt(const glm::vec3 &position) const
{
	if (groundHeightProvider_)
	{
		const float groundHeight = groundHeightProvider_(position.x, position.z);
		if (std::isfinite(groundHeight) &&
		    position.y < groundHeight + config_.collisionRadius)
		{
			return true;
		}
	}

	for (const StaticCollisionCylinder &obstacle : staticObstacles_)
	{
		if (obstacle.radius <= 0.0f || obstacle.height <= 0.0f)
		{
			continue;
		}
		const float expandedRadius = obstacle.radius + config_.collisionRadius;
		const glm::vec2 separation(position.x - obstacle.center.x,
		                           position.z - obstacle.center.y);
		if (glm::dot(separation, separation) > expandedRadius * expandedRadius)
		{
			continue;
		}
		const float obstacleTop = obstacle.baseY + obstacle.height;
		if (position.y + config_.collisionRadius > obstacle.baseY &&
		    position.y - config_.collisionRadius < obstacleTop)
		{
			return true;
		}
	}
	return false;
}
