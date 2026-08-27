#pragma once

#include <functional>

#include <glm/glm.hpp>

struct CaptureProjectileConfig
{
	float fullChargeSeconds = 1.2f;
	float minimumForwardSpeed = 9.5f;
	float maximumForwardSpeed = 17.0f;
	float minimumLiftSpeed = 4.0f;
	float maximumLiftSpeed = 6.0f;
	float gravity = 12.0f;
	float radius = 0.18f;
	float maximumFlightSeconds = 4.0f;
};

struct CaptureProjectileState
{
	glm::vec3 position = glm::vec3(0.0f);
	glm::vec3 velocity = glm::vec3(0.0f);
	float elapsedSeconds = 0.0f;
	bool active = false;
};

struct CaptureProjectileSegment
{
	glm::vec3 start = glm::vec3(0.0f);
	glm::vec3 end = glm::vec3(0.0f);
};

struct CaptureSweepHit
{
	bool hit = false;
	float fraction = 1.0f;
	glm::vec3 position = glm::vec3(0.0f);
	float quality = 0.0f;
};

struct CaptureCollisionCylinder
{
	glm::vec2 center = glm::vec2(0.0f);
	float radius = 1.0f;
	float baseY = 0.0f;
	float height = 1.0f;
};

using CaptureGroundHeightProvider = std::function<float(float, float)>;

float captureThrowChargeFraction(
	double heldSeconds,
	const CaptureProjectileConfig &config = CaptureProjectileConfig());
CaptureProjectileState launchCaptureProjectile(
	const glm::vec3 &origin, const glm::vec3 &aimDirection, float chargeFraction,
	const CaptureProjectileConfig &config = CaptureProjectileConfig());
glm::vec3 sampleCaptureProjectilePosition(
	const CaptureProjectileState &projectile, float futureSeconds,
	const CaptureProjectileConfig &config = CaptureProjectileConfig());
CaptureProjectileSegment advanceCaptureProjectile(
	CaptureProjectileState &projectile, float deltaSeconds,
	const CaptureProjectileConfig &config = CaptureProjectileConfig());

CaptureSweepHit sweepCaptureSphereAgainstSphere(
	const glm::vec3 &start, const glm::vec3 &end, float projectileRadius,
	const glm::vec3 &targetCenter, float targetRadius);
CaptureSweepHit sweepCaptureSphereAgainstTerrain(
	const glm::vec3 &start, const glm::vec3 &end, float projectileRadius,
	const CaptureGroundHeightProvider &groundHeightProvider);
CaptureSweepHit sweepCaptureSphereAgainstCylinder(
	const glm::vec3 &start, const glm::vec3 &end, float projectileRadius,
	const CaptureCollisionCylinder &cylinder);
