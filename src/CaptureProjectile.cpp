#include "CaptureProjectile.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr float EPSILON = 0.000001f;
constexpr int TERRAIN_SWEEP_SAMPLES = 16;
constexpr int TERRAIN_REFINEMENT_STEPS = 12;

float clampValue(float value, float minimum, float maximum)
{
	return std::max(minimum, std::min(maximum, value));
}

bool finiteVector(const glm::vec3 &value)
{
	return std::isfinite(value.x) && std::isfinite(value.y) &&
	       std::isfinite(value.z);
}

CaptureSweepHit noHit(const glm::vec3 &end)
{
	CaptureSweepHit result;
	result.position = end;
	return result;
}

bool intersectIntervals(float firstMinimum, float firstMaximum,
	                    float secondMinimum, float secondMaximum,
	                    float *entry, float *exit)
{
	*entry = std::max(firstMinimum, secondMinimum);
	*exit = std::min(firstMaximum, secondMaximum);
	return *entry <= *exit;
}
}

float captureThrowChargeFraction(double heldSeconds,
	                             const CaptureProjectileConfig &config)
{
	if (!std::isfinite(heldSeconds) || heldSeconds <= 0.0 ||
	    !std::isfinite(config.fullChargeSeconds) || config.fullChargeSeconds <= 0.0f)
	{
		return 0.0f;
	}
	return clampValue(static_cast<float>(heldSeconds) / config.fullChargeSeconds,
	                  0.0f, 1.0f);
}

CaptureProjectileState launchCaptureProjectile(
	const glm::vec3 &origin, const glm::vec3 &aimDirection, float chargeFraction,
	const CaptureProjectileConfig &config)
{
	CaptureProjectileState projectile;
	projectile.position = origin;
	if (!finiteVector(origin) || !finiteVector(aimDirection))
	{
		return projectile;
	}
	const float directionLength = glm::length(aimDirection);
	if (!std::isfinite(directionLength) || directionLength <= EPSILON)
	{
		return projectile;
	}

	const float charge = clampValue(
		std::isfinite(chargeFraction) ? chargeFraction : 0.0f, 0.0f, 1.0f);
	const float minimumForward = std::max(0.0f, config.minimumForwardSpeed);
	const float maximumForward = std::max(minimumForward, config.maximumForwardSpeed);
	const float minimumLift = std::max(0.0f, config.minimumLiftSpeed);
	const float maximumLift = std::max(minimumLift, config.maximumLiftSpeed);
	const float forwardSpeed =
		minimumForward + (maximumForward - minimumForward) * charge;
	const float liftSpeed = minimumLift + (maximumLift - minimumLift) * charge;
	projectile.velocity = aimDirection / directionLength * forwardSpeed +
	                      glm::vec3(0.0f, liftSpeed, 0.0f);
	projectile.active = forwardSpeed > EPSILON &&
	                    std::isfinite(config.maximumFlightSeconds) &&
	                    config.maximumFlightSeconds > 0.0f;
	return projectile;
}

glm::vec3 sampleCaptureProjectilePosition(
	const CaptureProjectileState &projectile, float futureSeconds,
	const CaptureProjectileConfig &config)
{
	const float time = std::isfinite(futureSeconds)
	                       ? std::max(0.0f, futureSeconds)
	                       : 0.0f;
	const float gravity = std::isfinite(config.gravity)
	                          ? std::max(0.0f, config.gravity)
	                          : 0.0f;
	return projectile.position + projectile.velocity * time +
	       glm::vec3(0.0f, -0.5f * gravity * time * time, 0.0f);
}

CaptureProjectileSegment advanceCaptureProjectile(
	CaptureProjectileState &projectile, float deltaSeconds,
	const CaptureProjectileConfig &config)
{
	CaptureProjectileSegment segment;
	segment.start = projectile.position;
	segment.end = projectile.position;
	if (!projectile.active || !std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f)
	{
		return segment;
	}

	const float maximumFlight = std::max(0.0f, config.maximumFlightSeconds);
	const float remaining = maximumFlight - projectile.elapsedSeconds;
	if (remaining <= 0.0f)
	{
		projectile.active = false;
		return segment;
	}
	const float step = std::min(deltaSeconds, remaining);
	segment.end = sampleCaptureProjectilePosition(projectile, step, config);
	const float gravity = std::isfinite(config.gravity)
	                          ? std::max(0.0f, config.gravity)
	                          : 0.0f;
	projectile.position = segment.end;
	projectile.velocity.y -= gravity * step;
	projectile.elapsedSeconds += step;
	if (projectile.elapsedSeconds >= maximumFlight)
	{
		projectile.active = false;
	}
	return segment;
}

CaptureSweepHit sweepCaptureSphereAgainstSphere(
	const glm::vec3 &start, const glm::vec3 &end, float projectileRadius,
	const glm::vec3 &targetCenter, float targetRadius)
{
	if (!finiteVector(start) || !finiteVector(end) || !finiteVector(targetCenter) ||
	    !std::isfinite(projectileRadius) || !std::isfinite(targetRadius) ||
	    projectileRadius < 0.0f || targetRadius <= 0.0f)
	{
		return noHit(end);
	}
	const float combinedRadius = projectileRadius + targetRadius;
	const glm::vec3 segment = end - start;
	const glm::vec3 offset = start - targetCenter;
	const float segmentLengthSquared = glm::dot(segment, segment);
	const float startDistanceSquared = glm::dot(offset, offset);
	if (segmentLengthSquared <= EPSILON)
	{
		if (startDistanceSquared > combinedRadius * combinedRadius)
		{
			return noHit(end);
		}
		CaptureSweepHit hit;
		hit.hit = true;
		hit.fraction = 0.0f;
		hit.position = start;
		hit.quality = clampValue(
			1.0f - std::sqrt(startDistanceSquared) / combinedRadius, 0.0f, 1.0f);
		return hit;
	}

	const float closestFraction = clampValue(
		glm::dot(targetCenter - start, segment) / segmentLengthSquared, 0.0f, 1.0f);
	const float closestDistance =
		glm::length(start + segment * closestFraction - targetCenter);
	const float quality =
		clampValue(1.0f - closestDistance / combinedRadius, 0.0f, 1.0f);
	const float b = glm::dot(offset, segment);
	const float c = startDistanceSquared - combinedRadius * combinedRadius;
	if (c <= 0.0f)
	{
		CaptureSweepHit hit;
		hit.hit = true;
		hit.fraction = 0.0f;
		hit.position = start;
		hit.quality = quality;
		return hit;
	}
	const float discriminant = b * b - segmentLengthSquared * c;
	if (discriminant < 0.0f)
	{
		return noHit(end);
	}
	const float fraction =
		(-b - std::sqrt(discriminant)) / segmentLengthSquared;
	if (fraction < 0.0f || fraction > 1.0f)
	{
		return noHit(end);
	}
	CaptureSweepHit hit;
	hit.hit = true;
	hit.fraction = fraction;
	hit.position = start + segment * fraction;
	hit.quality = quality;
	return hit;
}

CaptureSweepHit sweepCaptureSphereAgainstTerrain(
	const glm::vec3 &start, const glm::vec3 &end, float projectileRadius,
	const CaptureGroundHeightProvider &groundHeightProvider)
{
	if (!finiteVector(start) || !finiteVector(end) ||
	    !std::isfinite(projectileRadius) || projectileRadius < 0.0f ||
	    !groundHeightProvider)
	{
		return noHit(end);
	}
	const glm::vec3 segment = end - start;
	auto clearanceAt = [&](float fraction) {
		const glm::vec3 position = start + segment * fraction;
		const float height = groundHeightProvider(position.x, position.z);
		return std::isfinite(height)
		           ? position.y - projectileRadius - height
		           : std::numeric_limits<float>::infinity();
	};
	if (clearanceAt(0.0f) <= 0.0f)
	{
		CaptureSweepHit hit;
		hit.hit = true;
		hit.fraction = 0.0f;
		hit.position = start;
		return hit;
	}

	float previousFraction = 0.0f;
	for (int sample = 1; sample <= TERRAIN_SWEEP_SAMPLES; ++sample)
	{
		const float fraction = static_cast<float>(sample) /
		                       static_cast<float>(TERRAIN_SWEEP_SAMPLES);
		if (clearanceAt(fraction) > 0.0f)
		{
			previousFraction = fraction;
			continue;
		}
		float clearFraction = previousFraction;
		float blockedFraction = fraction;
		for (int refinement = 0; refinement < TERRAIN_REFINEMENT_STEPS;
		     ++refinement)
		{
			const float midpoint = (clearFraction + blockedFraction) * 0.5f;
			if (clearanceAt(midpoint) <= 0.0f)
			{
				blockedFraction = midpoint;
			}
			else
			{
				clearFraction = midpoint;
			}
		}
		CaptureSweepHit hit;
		hit.hit = true;
		hit.fraction = blockedFraction;
		hit.position = start + segment * blockedFraction;
		return hit;
	}
	return noHit(end);
}

CaptureSweepHit sweepCaptureSphereAgainstCylinder(
	const glm::vec3 &start, const glm::vec3 &end, float projectileRadius,
	const CaptureCollisionCylinder &cylinder)
{
	if (!finiteVector(start) || !finiteVector(end) ||
	    !std::isfinite(projectileRadius) || projectileRadius < 0.0f ||
	    !std::isfinite(cylinder.center.x) || !std::isfinite(cylinder.center.y) ||
	    !std::isfinite(cylinder.radius) || !std::isfinite(cylinder.baseY) ||
	    !std::isfinite(cylinder.height) || cylinder.radius <= 0.0f ||
	    cylinder.height <= 0.0f)
	{
		return noHit(end);
	}

	const glm::vec3 segment = end - start;
	const glm::vec2 horizontalStart(start.x - cylinder.center.x,
	                                start.z - cylinder.center.y);
	const glm::vec2 horizontalDelta(segment.x, segment.z);
	const float expandedRadius = cylinder.radius + projectileRadius;
	float horizontalEntry = 0.0f;
	float horizontalExit = 1.0f;
	const float horizontalLengthSquared =
		glm::dot(horizontalDelta, horizontalDelta);
	if (horizontalLengthSquared <= EPSILON)
	{
		if (glm::dot(horizontalStart, horizontalStart) >
		    expandedRadius * expandedRadius)
		{
			return noHit(end);
		}
	}
	else
	{
		const float b = glm::dot(horizontalStart, horizontalDelta);
		const float c = glm::dot(horizontalStart, horizontalStart) -
		                expandedRadius * expandedRadius;
		const float discriminant = b * b - horizontalLengthSquared * c;
		if (discriminant < 0.0f)
		{
			return noHit(end);
		}
		const float root = std::sqrt(discriminant);
		horizontalEntry = (-b - root) / horizontalLengthSquared;
		horizontalExit = (-b + root) / horizontalLengthSquared;
	}

	float verticalEntry = 0.0f;
	float verticalExit = 1.0f;
	const float minimumY = cylinder.baseY - projectileRadius;
	const float maximumY = cylinder.baseY + cylinder.height + projectileRadius;
	if (std::fabs(segment.y) <= EPSILON)
	{
		if (start.y < minimumY || start.y > maximumY)
		{
			return noHit(end);
		}
	}
	else
	{
		const float first = (minimumY - start.y) / segment.y;
		const float second = (maximumY - start.y) / segment.y;
		verticalEntry = std::min(first, second);
		verticalExit = std::max(first, second);
	}

	float entry = 0.0f;
	float exit = 1.0f;
	if (!intersectIntervals(horizontalEntry, horizontalExit, verticalEntry,
	                        verticalExit, &entry, &exit) ||
	    !intersectIntervals(entry, exit, 0.0f, 1.0f, &entry, &exit))
	{
		return noHit(end);
	}
	CaptureSweepHit hit;
	hit.hit = true;
	hit.fraction = entry;
	hit.position = start + segment * entry;
	return hit;
}
