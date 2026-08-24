#include "PokemonNavigation.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

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
		          << ", got " << actual << ')' << std::endl;
		++failures;
	}
}

void testUnobstructedVelocityIsPreserved()
{
	const glm::vec3 desired(1.2f, 0.0f, -2.4f);
	const glm::vec3 steered = steerGroundPokemonVelocity(
		glm::vec3(0.0f), desired, 0.55f, 7, {});
	expectNear(glm::distance(steered, desired), 0.0f, 0.0001f,
	           "navigation leaves an unobstructed trajectory unchanged");
}

void testRockAheadCreatesAPlanarTurnWithoutChangingSpeed()
{
	const glm::vec3 desired(0.0f, 0.0f, -2.0f);
	const std::vector<PokemonNavigationBlocker> blockers = {
		{-100, glm::vec2(0.0f, -1.3f), 0.78f},
	};
	const glm::vec3 steered = steerGroundPokemonVelocity(
		glm::vec3(0.0f), desired, 0.55f, 7, blockers);
	expectTrue(std::fabs(steered.x) > 0.05f && steered.z < -0.1f,
	           "a rock directly ahead produces a forward sideways turn");
	expectNear(glm::length(glm::vec2(steered.x, steered.z)), 2.0f, 0.0001f,
	           "steering preserves the creature's configured movement speed");
}

void testResolutionPreventsRockOverlap()
{
	const std::vector<PokemonNavigationBlocker> blockers = {
		{-100, glm::vec2(0.0f, -1.0f), 0.8f},
	};
	const PokemonNavigationResult result = resolveGroundPokemonPosition(
		glm::vec2(0.0f, -1.0f), 0.55f, 7, blockers);
	expectTrue(result.collided, "overlapping a rock reports a navigation collision");
	expectTrue(glm::length(result.position - blockers[0].center) >= 1.45f,
	           "resolution moves the creature outside the rock clearance radius");
	expectNear(glm::length(result.collisionNormal), 1.0f, 0.0001f,
	           "resolution returns a usable collision normal");
}

void testOtherPokemonBlocksButSelfDoesNot()
{
	const std::vector<PokemonNavigationBlocker> blockers = {
		{7, glm::vec2(0.0f, -1.0f), 0.55f},
		{8, glm::vec2(0.8f, 0.0f), 0.55f},
	};
	const PokemonNavigationResult result = resolveGroundPokemonPosition(
		glm::vec2(0.8f, 0.0f), 0.55f, 7, blockers);
	expectTrue(result.collided,
	           "a nearby ground Pokemon is a collision blocker");
	expectTrue(glm::length(result.position - blockers[1].center) >= 1.2f,
	           "creature separation includes a small navigation clearance");
}
}

int main()
{
	testUnobstructedVelocityIsPreserved();
	testRockAheadCreatesAPlanarTurnWithoutChangingSpeed();
	testResolutionPreventsRockOverlap();
	testOtherPokemonBlocksButSelfDoesNot();
	if (failures != 0)
	{
		return 1;
	}
	std::cout << "Pokemon navigation tests passed" << std::endl;
	return 0;
}
