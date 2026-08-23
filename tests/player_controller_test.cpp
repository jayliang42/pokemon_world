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

void testDodgeStartsWithBurstAndInvulnerability()
{
	PlayerPhysicsConfig config;
	config.dodgeSpeed = 12.0f;
	config.dodgeDuration = 0.2f;
	config.dodgeCooldown = 1.0f;
	config.dodgeInvulnerability = 0.35f;
	PlayerController player(config);
	PlayerInput input;

	expectTrue(player.requestDodge(), "a ready player can request a dodge");
	PlayerMotionEvents events = player.update(input, 0.05f);
	expectTrue(events.dodgeStarted, "accepted dodge reports a start event");
	expectNear(player.position().z, -0.6f, 0.0001f,
	           "dodge immediately moves at the configured burst speed");
	expectNear(glm::length(glm::vec2(player.velocity().x, player.velocity().z)),
	           12.0f, 0.0001f, "dodge velocity uses its independent speed");
	expectTrue(player.isDodging(), "dodge remains active for its configured duration");
	expectTrue(player.isInvulnerable(), "dodge starts an invulnerability window");
	expectNear(player.dodgeCooldownRemaining(), 1.0f, 0.0001f,
	           "dodge starts its independent cooldown");
	expectNear(player.dodgeCooldownFraction(), 1.0f, 0.0001f,
	           "fresh dodge reports a full cooldown fraction");
	expectTrue(!player.requestDodge(), "an active dodge cannot be queued again");
}

void testDodgeCooldownExpiresAndResetClearsTransientState()
{
	PlayerPhysicsConfig config;
	config.dodgeDuration = 0.1f;
	config.dodgeCooldown = 0.3f;
	config.dodgeInvulnerability = 0.15f;
	PlayerController player(config);
	PlayerInput input;

	player.requestDodge();
	player.update(input, 0.05f);
	for (int frame = 0; frame < 7; ++frame)
	{
		player.update(input, 0.05f);
	}
	expectTrue(!player.isDodging(), "dodge duration expires independently");
	expectTrue(!player.isInvulnerable(), "dodge invulnerability expires independently");
	expectNear(player.dodgeCooldownRemaining(), 0.0f, 0.0001f,
	           "dodge cooldown counts down to ready");
	expectTrue(player.requestDodge(), "dodge can be requested again after cooldown");
	player.update(input, 0.05f);

	player.reset(glm::vec3(2.0f, 0.0f, 1.0f));
	expectTrue(!player.isDodging(), "reset stops an active dodge");
	expectTrue(!player.isInvulnerable(), "reset clears dodge invulnerability");
	expectNear(player.dodgeCooldownRemaining(), 0.0f, 0.0001f,
	           "reset clears dodge cooldown");
	expectTrue(player.requestDodge(), "reset returns dodge to a ready state");
}

void testDodgeCannotPenetrateStaticObstacleOrReapplyInwardSpeed()
{
	PlayerPhysicsConfig config;
	config.dodgeSpeed = 13.0f;
	config.dodgeDuration = 0.3f;
	PlayerController player(config);
	StaticCollisionCylinder obstacle;
	obstacle.center = glm::vec2(0.0f, -3.0f);
	obstacle.radius = 1.0f;
	obstacle.baseY = 0.0f;
	obstacle.height = 2.0f;
	player.setStaticObstacles({obstacle});
	PlayerInput input;

	player.requestDodge();
	int obstacleEvents = 0;
	for (int frame = 0; frame < 8; ++frame)
	{
		if (player.update(input, 0.05f).hitObstacle)
		{
			++obstacleEvents;
		}
		const float distance = glm::length(glm::vec2(
			player.position().x, player.position().z + 3.0f));
		expectTrue(distance >= 1.7999f,
		           "dodge remains outside the obstacle on every frame");
	}
	expectNear(player.position().z, -1.2f, 0.0002f,
	           "head-on dodge stops at the obstacle surface");
	expectNear(player.velocity().z, 0.0f, 0.0001f,
	           "collision removes dodge speed directed into the obstacle");
	expectTrue(obstacleEvents == 1,
	           "dodge obstacle contact is debounced while touching");
}

void testDodgeRespectsPlayerRadiusAtFieldBoundary()
{
	PlayerPhysicsConfig config;
	config.dodgeSpeed = 13.0f;
	PlayerController player(config);
	player.reset(glm::vec3(0.0f, 0.0f, -47.0f));
	PlayerInput input;

	player.requestDodge();
	PlayerMotionEvents events = player.update(input, 0.05f);
	expectTrue(events.hitBoundary, "dodge reports its first boundary collision");
	expectNear(player.position().z, -47.2f, 0.0001f,
	           "dodge preserves the player collision radius at the field edge");
	expectNear(player.velocity().z, 0.0f, 0.0001f,
	           "field boundary removes outward dodge velocity");
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
	testDodgeStartsWithBurstAndInvulnerability();
	testDodgeCooldownExpiresAndResetClearsTransientState();
	testDodgeCannotPenetrateStaticObstacleOrReapplyInwardSpeed();
	testDodgeRespectsPlayerRadiusAtFieldBoundary();

	if (failures != 0)
	{
		std::cerr << failures << " player controller test(s) failed" << std::endl;
		return 1;
	}

	std::cout << "All player controller tests passed" << std::endl;
	return 0;
}
