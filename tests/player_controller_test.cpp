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
