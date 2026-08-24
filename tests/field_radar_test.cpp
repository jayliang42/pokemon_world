#include "FieldRadar.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace
{
constexpr float PI = 3.14159265358979323846f;
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

void testRadarSkipsUnavailableAndFindsNearestSample()
{
	const std::vector<FieldRadarCandidate> candidates = {
		{2, false, glm::vec3(0.0f, 0.0f, -1.0f)},
		{7, true, glm::vec3(0.0f, 0.0f, -12.0f)},
		{3, true, glm::vec3(0.0f, 20.0f, -5.0f)},
	};
	const FieldRadarContact contact =
		selectNearestFieldRadarContact(glm::vec3(0.0f), 0.0f, candidates);
	expectTrue(contact.valid() && contact.id == 3,
	           "the radar skips unavailable Pokemon and chooses the nearest available sample");
	expectNear(contact.distance, 5.0f, 0.0001f,
	           "radar distance uses the field plane instead of flight altitude");
}

void testRadarReportsDirectionsRelativeToThePlayer()
{
	const glm::vec3 playerPosition(0.0f);
	const FieldRadarContact ahead = selectNearestFieldRadarContact(
		playerPosition, 0.0f, {{1, true, glm::vec3(0.0f, 0.0f, -8.0f)}});
	const FieldRadarContact left = selectNearestFieldRadarContact(
		playerPosition, 0.0f, {{2, true, glm::vec3(-8.0f, 0.0f, 0.0f)}});
	const FieldRadarContact right = selectNearestFieldRadarContact(
		playerPosition, 0.0f, {{3, true, glm::vec3(8.0f, 0.0f, 0.0f)}});
	const FieldRadarContact turnedAhead = selectNearestFieldRadarContact(
		playerPosition, PI * 0.5f, {{4, true, glm::vec3(-8.0f, 0.0f, 0.0f)}});

	expectNear(ahead.bearingRadians, 0.0f, 0.0001f,
	           "a sample directly ahead is centered on the radar");
	expectNear(left.bearingRadians, -PI * 0.5f, 0.0001f,
	           "a sample left of the player uses a negative radar bearing");
	expectNear(right.bearingRadians, PI * 0.5f, 0.0001f,
	           "a sample right of the player uses a positive radar bearing");
	expectNear(turnedAhead.bearingRadians, 0.0f, 0.0001f,
	           "the bearing tracks the player's current yaw");
}

void testRadarUsesStableCandidateOrderingForEqualDistance()
{
	const FieldRadarContact contact = selectNearestFieldRadarContact(
		glm::vec3(0.0f), 0.0f,
		{{8, true, glm::vec3(-4.0f, 0.0f, 0.0f)},
		 {5, true, glm::vec3(4.0f, 0.0f, 0.0f)}});
	expectTrue(contact.valid() && contact.id == 5,
	           "equal-distance contacts use their stable lowest identifier");
}
}

int main()
{
	testRadarSkipsUnavailableAndFindsNearestSample();
	testRadarReportsDirectionsRelativeToThePlayer();
	testRadarUsesStableCandidateOrderingForEqualDistance();
	if (failures != 0)
	{
		return 1;
	}
	std::cout << "Field radar tests passed" << std::endl;
	return 0;
}
