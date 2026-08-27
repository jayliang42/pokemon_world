#include "CaptureProjectile.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{
int failures = 0;

void expectTrue(bool condition, const std::string &message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << std::endl;
		++failures;
	}
}

void expectNear(float actual, float expected, float tolerance,
	            const std::string &message)
{
	if (std::fabs(actual - expected) > tolerance)
	{
		std::cerr << "FAIL: " << message << " (expected " << expected
		          << ", got " << actual << ")" << std::endl;
		++failures;
	}
}

void expectVectorNear(const glm::vec3 &actual, const glm::vec3 &expected,
	                  float tolerance, const std::string &message)
{
	expectNear(actual.x, expected.x, tolerance, message + " x");
	expectNear(actual.y, expected.y, tolerance, message + " y");
	expectNear(actual.z, expected.z, tolerance, message + " z");
}

void testChargeAndLaunchAreBounded()
{
	CaptureProjectileConfig config;
	expectNear(captureThrowChargeFraction(-1.0, config), 0.0f, 0.0f,
	           "negative hold time has no charge");
	expectNear(captureThrowChargeFraction(config.fullChargeSeconds * 0.5, config),
	           0.5f, 0.0001f, "half the charge time produces half charge");
	expectNear(captureThrowChargeFraction(config.fullChargeSeconds * 4.0, config),
	           1.0f, 0.0f, "charge is capped at one");

	const CaptureProjectileState minimum = launchCaptureProjectile(
		glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(10.0f, 0.0f, 0.0f), 0.0f,
		config);
	const CaptureProjectileState maximum = launchCaptureProjectile(
		glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(1.0f, 0.0f, 0.0f), 1.0f,
		config);
	expectTrue(minimum.active && maximum.active,
	           "valid aim directions launch active projectiles");
	expectNear(minimum.velocity.x, config.minimumForwardSpeed, 0.0001f,
	           "minimum charge uses minimum forward speed");
	expectNear(maximum.velocity.x, config.maximumForwardSpeed, 0.0001f,
	           "maximum charge uses maximum forward speed");
	expectNear(maximum.velocity.y, config.maximumLiftSpeed, 0.0001f,
	           "maximum charge uses maximum lift");
	expectTrue(!launchCaptureProjectile(glm::vec3(0.0f), glm::vec3(0.0f), 0.5f,
	                                  config)
	                .active,
	           "a zero aim vector fails closed");
}

void testPredictionMatchesIncrementalBallistics()
{
	CaptureProjectileConfig config;
	CaptureProjectileState projectile = launchCaptureProjectile(
		glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), 1.0f,
		config);
	const glm::vec3 predicted =
		sampleCaptureProjectilePosition(projectile, 1.0f, config);
	const CaptureProjectileSegment first =
		advanceCaptureProjectile(projectile, 0.4f, config);
	const CaptureProjectileSegment second =
		advanceCaptureProjectile(projectile, 0.6f, config);
	expectVectorNear(first.start, glm::vec3(0.0f, 1.0f, 0.0f), 0.0001f,
	                 "the first swept segment starts at launch");
	expectVectorNear(second.end, predicted, 0.0002f,
	                 "incremental simulation matches the prediction arc");
	expectNear(projectile.elapsedSeconds, 1.0f, 0.0001f,
	           "projectile flight time accumulates deterministically");
}

void testSweptSphereDetectsCenteredAndGrazingTargets()
{
	const glm::vec3 start(0.0f, 0.0f, 0.0f);
	const glm::vec3 end(10.0f, 0.0f, 0.0f);
	const CaptureSweepHit centered = sweepCaptureSphereAgainstSphere(
		start, end, 0.25f, glm::vec3(5.0f, 0.0f, 0.0f), 1.0f);
	const CaptureSweepHit grazing = sweepCaptureSphereAgainstSphere(
		start, end, 0.25f, glm::vec3(5.0f, 0.0f, 1.0f), 1.0f);
	const CaptureSweepHit missed = sweepCaptureSphereAgainstSphere(
		start, end, 0.25f, glm::vec3(5.0f, 0.0f, 1.3f), 1.0f);
	expectTrue(centered.hit, "a fast projectile cannot tunnel through a target");
	expectNear(centered.fraction, 0.375f, 0.0001f,
	           "sphere sweep reports the first contact point");
	expectNear(centered.quality, 1.0f, 0.0001f,
	           "a center-line throw has perfect quality");
	expectTrue(grazing.hit && grazing.quality > 0.0f && grazing.quality < 0.3f,
	           "a grazing hit reports lower precision");
	expectTrue(!missed.hit, "a near miss remains a real miss");
}

void testTerrainSweepFindsFirstGroundContact()
{
	const CaptureSweepHit hit = sweepCaptureSphereAgainstTerrain(
		glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(0.0f, -2.0f, 0.0f), 0.25f,
		[](float, float) { return 0.0f; });
	expectTrue(hit.hit, "a downward swept ball hits flat terrain");
	expectNear(hit.fraction, 0.4375f, 0.001f,
	           "terrain sweep refines the first radius-aware contact");
	expectNear(hit.position.y, 0.25f, 0.004f,
	           "terrain impact leaves the ball above the surface");

	const CaptureSweepHit clear = sweepCaptureSphereAgainstTerrain(
		glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(4.0f, 2.0f, 0.0f), 0.25f,
		[](float, float) { return 0.0f; });
	expectTrue(!clear.hit, "a segment above terrain stays clear");
}

void testCylinderSweepBlocksRocksContinuously()
{
	CaptureCollisionCylinder rock;
	rock.center = glm::vec2(0.0f);
	rock.radius = 1.0f;
	rock.baseY = 0.0f;
	rock.height = 2.0f;
	const CaptureSweepHit hit = sweepCaptureSphereAgainstCylinder(
		glm::vec3(-3.0f, 1.0f, 0.0f), glm::vec3(3.0f, 1.0f, 0.0f), 0.2f,
		rock);
	const CaptureSweepHit above = sweepCaptureSphereAgainstCylinder(
		glm::vec3(-3.0f, 3.0f, 0.0f), glm::vec3(3.0f, 3.0f, 0.0f), 0.2f,
		rock);
	expectTrue(hit.hit, "a swept ball cannot tunnel through a rock collider");
	expectNear(hit.fraction, 0.3f, 0.0002f,
	           "cylinder sweep reports its first expanded-radius contact");
	expectTrue(!above.hit, "a ball above the expanded rock height is clear");
}
}

int main()
{
	testChargeAndLaunchAreBounded();
	testPredictionMatchesIncrementalBallistics();
	testSweptSphereDetectsCenteredAndGrazingTargets();
	testTerrainSweepFindsFirstGroundContact();
	testCylinderSweepBlocksRocksContinuously();

	if (failures != 0)
	{
		std::cerr << failures << " capture projectile test(s) failed" << std::endl;
		return 1;
	}
	std::cout << "All capture projectile tests passed" << std::endl;
	return 0;
}
