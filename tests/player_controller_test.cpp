#include "PlayerController.h"

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

void expectNear(float actual, float expected, float tolerance, const std::string &message)
{
	if (std::fabs(actual - expected) > tolerance)
	{
		std::cerr << "FAIL: " << message << " (expected " << expected
		          << ", got " << actual << ")" << std::endl;
		++failures;
	}
}

void testSmoothHorizontalAccelerationAndBraking()
{
	PlayerController player;
	PlayerInput input;
	input.forward = 1.0f;

	player.update(input, 0.05f);
	float firstFrameSpeed = glm::length(glm::vec2(player.velocity().x, player.velocity().z));
	expectTrue(firstFrameSpeed > 0.0f && firstFrameSpeed < 7.0f,
	           "forward input accelerates instead of jumping to maximum speed");

	for (int i = 0; i < 30; ++i)
	{
		player.update(input, 0.05f);
	}
	float cruisingSpeed = glm::length(glm::vec2(player.velocity().x, player.velocity().z));
	expectNear(cruisingSpeed, 7.0f, 0.05f, "forward speed reaches the configured cap");

	input.forward = 0.0f;
	for (int i = 0; i < 10; ++i)
	{
		player.update(input, 0.05f);
	}
	float stoppedSpeed = glm::length(glm::vec2(player.velocity().x, player.velocity().z));
	expectNear(stoppedSpeed, 0.0f, 0.001f, "releasing movement brakes to a full stop");
}

void testGravityAcceleratesAndLandsOnce()
{
	PlayerController player;
	player.reset(glm::vec3(0.0f, 5.0f, 0.0f));
	player.setGravityEnabled(true);
	PlayerInput input;

	player.update(input, 0.05f);
	float firstFallSpeed = player.verticalVelocity();
	player.update(input, 0.05f);
	float secondFallSpeed = player.verticalVelocity();
	expectTrue(secondFallSpeed < firstFallSpeed,
	           "gravity accelerates downward instead of applying a fixed descent speed");

	int landingEvents = 0;
	for (int i = 0; i < 200; ++i)
	{
		PlayerMotionEvents events = player.update(input, 0.05f);
		if (events.landed)
		{
			++landingEvents;
		}
	}
	expectNear(player.position().y, 0.0f, 0.0001f, "ground collision clamps player height");
	expectNear(player.verticalVelocity(), 0.0f, 0.0001f, "ground collision clears downward velocity");
	expectTrue(player.grounded(), "player reports grounded after landing");
	expectTrue(landingEvents == 1, "landing event fires once per landing");
}

void testTerrainGroundPreventsUphillPenetration()
{
	PlayerController player;
	player.setGroundHeightProvider([](float, float worldZ) {
		return worldZ < -0.1f ? 2.0f : 1.0f;
	});
	player.reset(glm::vec3(0.0f, 1.0f, 0.0f));
	PlayerInput input;
	input.forward = 1.0f;

	for (int i = 0; i < 20; ++i)
	{
		player.update(input, 0.05f);
	}
	expectTrue(player.position().z < -0.1f, "player moves onto the raised part of the terrain");
	expectNear(player.position().y, 2.0f, 0.0001f,
	           "terrain collision lifts a grounded player instead of allowing penetration");
	expectTrue(player.grounded(), "walking uphill keeps the player grounded");
}

void testGravityLandsOnLocalTerrainHeightOnce()
{
	PlayerController player;
	player.setGroundHeightProvider([](float, float) { return 3.0f; });
	player.reset(glm::vec3(0.0f, 8.0f, 0.0f));
	player.setGravityEnabled(true);
	PlayerInput input;
	int landingEvents = 0;

	for (int i = 0; i < 200; ++i)
	{
		if (player.update(input, 0.05f).landed)
		{
			++landingEvents;
		}
	}
	expectNear(player.position().y, 3.0f, 0.0001f,
	           "gravity lands on the sampled terrain instead of the global zero plane");
	expectNear(player.verticalVelocity(), 0.0f, 0.0001f,
	           "terrain landing clears downward velocity");
	expectTrue(player.grounded(), "terrain landing reports grounded state");
	expectTrue(landingEvents == 1, "terrain landing event fires once per landing");
}

void testFlightCeilingFollowsLocalTerrainHeight()
{
	PlayerController player;
	player.setGroundHeightProvider([](float, float) { return 4.0f; });
	player.reset(glm::vec3(0.0f, 4.0f, 0.0f));
	PlayerInput input;
	input.vertical = 1.0f;

	for (int i = 0; i < 120; ++i)
	{
		player.update(input, 0.05f);
	}
	expectNear(player.position().y, 36.0f, 0.0001f,
	           "maximum altitude is measured above the local terrain");
}

void testStaticObstacleStopsAndDebouncesContact()
{
	PlayerController player;
	StaticCollisionCylinder obstacle;
	obstacle.center = glm::vec2(0.0f, -3.0f);
	obstacle.radius = 1.0f;
	obstacle.baseY = 0.0f;
	obstacle.height = 2.0f;
	player.setStaticObstacles({obstacle});
	PlayerInput input;
	input.forward = 1.0f;
	int obstacleEvents = 0;

	for (int i = 0; i < 120; ++i)
	{
		if (player.update(input, 0.05f).hitObstacle)
		{
			++obstacleEvents;
		}
	}
	const float centerDistance = glm::length(glm::vec2(player.position().x,
	                                                   player.position().z + 3.0f));
	expectTrue(centerDistance >= 1.7999f,
	           "player collision radius never penetrates a static obstacle");
	expectTrue(player.position().z > -1.21f,
	           "head-on movement stops at the obstacle surface");
	expectTrue(obstacleEvents == 1, "static obstacle contact is reported once until separation");
}

void testStaticObstaclePreservesTangentialSliding()
{
	PlayerController player;
	StaticCollisionCylinder obstacle;
	obstacle.center = glm::vec2(0.0f, -3.0f);
	obstacle.radius = 1.0f;
	obstacle.baseY = 0.0f;
	obstacle.height = 2.0f;
	player.setStaticObstacles({obstacle});
	player.reset(glm::vec3(-1.4f, 0.0f, 0.0f), -0.2f);
	PlayerInput input;
	input.forward = 1.0f;

	for (int i = 0; i < 80; ++i)
	{
		player.update(input, 0.05f);
	}
	const float centerDistance = glm::length(glm::vec2(player.position().x,
	                                                   player.position().z + 3.0f));
	expectTrue(centerDistance >= 1.7999f, "sliding motion remains outside the obstacle");
	expectTrue(player.position().z < -3.0f,
	           "collision removes inward velocity but preserves motion around the obstacle");
}

void testPlayerCanFlyAboveStaticObstacle()
{
	PlayerController player;
	StaticCollisionCylinder obstacle;
	obstacle.center = glm::vec2(0.0f, -3.0f);
	obstacle.radius = 1.0f;
	obstacle.baseY = 0.0f;
	obstacle.height = 2.0f;
	player.setStaticObstacles({obstacle});
	player.reset(glm::vec3(0.0f, 3.0f, 0.0f));
	PlayerInput input;
	input.forward = 1.0f;

	for (int i = 0; i < 40; ++i)
	{
		player.update(input, 0.05f);
	}
	expectTrue(player.position().z < -4.0f,
	           "player above an obstacle can fly across its horizontal footprint");
}

void testBoundaryUsesCollisionRadiusAndSlides()
{
	PlayerController player;
	player.reset(glm::vec3(47.0f, 0.0f, 0.0f), -1.2f);
	PlayerInput input;
	input.forward = 1.0f;

	int boundaryEvents = 0;
	for (int i = 0; i < 80; ++i)
	{
		PlayerMotionEvents events = player.update(input, 0.05f);
		if (events.hitBoundary)
		{
			++boundaryEvents;
		}
	}
	expectTrue(boundaryEvents == 1, "field boundary collision is reported once per contact");
	expectTrue(player.position().x <= 47.2001f, "collision radius stays inside the field edge");
	expectTrue(std::fabs(player.position().z) > 0.1f,
	           "boundary collision preserves tangential motion instead of freezing the player");
}

void testCeilingStopsAscent()
{
	PlayerController player;
	PlayerInput input;
	input.vertical = 1.0f;
	bool hitCeiling = false;

	for (int i = 0; i < 120; ++i)
	{
		PlayerMotionEvents events = player.update(input, 0.05f);
		hitCeiling = hitCeiling || events.hitCeiling;
	}
	expectTrue(hitCeiling, "altitude ceiling collision is reported");
	expectNear(player.position().y, 32.0f, 0.0001f, "altitude is clamped at the ceiling");
	expectNear(player.verticalVelocity(), 0.0f, 0.0001f, "ceiling clears upward velocity");
}

void testLargeFrameIsClamped()
{
	PlayerController regular;
	PlayerController stalled;
	PlayerInput input;
	input.forward = 1.0f;
	regular.update(input, 0.05f);
	stalled.update(input, 2.0f);

	expectNear(stalled.position().x, regular.position().x, 0.0001f,
	           "stalled frame uses the same clamped horizontal step");
	expectNear(stalled.position().z, regular.position().z, 0.0001f,
	           "stalled frame cannot teleport the player");
}
}

int main()
{
	testSmoothHorizontalAccelerationAndBraking();
	testGravityAcceleratesAndLandsOnce();
	testTerrainGroundPreventsUphillPenetration();
	testGravityLandsOnLocalTerrainHeightOnce();
	testFlightCeilingFollowsLocalTerrainHeight();
	testStaticObstacleStopsAndDebouncesContact();
	testStaticObstaclePreservesTangentialSliding();
	testPlayerCanFlyAboveStaticObstacle();
	testBoundaryUsesCollisionRadiusAndSlides();
	testCeilingStopsAscent();
	testLargeFrameIsClamped();

	if (failures != 0)
	{
		std::cerr << failures << " player controller test(s) failed" << std::endl;
		return 1;
	}

	std::cout << "All player controller tests passed" << std::endl;
	return 0;
}
