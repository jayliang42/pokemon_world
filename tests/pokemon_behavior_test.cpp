#include "Pokemon.h"

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

float horizontalDistance(const glm::vec3 &first, const glm::vec3 &second)
{
	return glm::length(glm::vec2(first.x - second.x, first.z - second.z));
}

void testDeterministicSpawnAvoidsPlayerAndFieldEdge()
{
	Pokemon first(0, 7, 12345u);
	Pokemon second(0, 7, 12345u);
	expectNear(glm::distance(first.getPos(), second.getPos()), 0.0f, 0.0001f,
	           "same seed produces the same spawn and behavior setup");
	expectTrue(horizontalDistance(first.getPos(), glm::vec3(0.0f)) >= 8.0f,
	           "ground Pokemon do not spawn directly on the player");
	expectTrue(std::fabs(first.getPos().x) <= 44.0f && std::fabs(first.getPos().z) <= 44.0f,
	           "spawn leaves room before the field boundary");
}

void testNearbyPlayerTriggersSmoothFleeMotion()
{
	Pokemon pokemon(0, 2, 99u);
	pokemon.setPosition(glm::vec3(2.0f, 0.0f, 0.0f));
	const glm::vec3 playerPosition(0.0f);
	const float startingDistance = horizontalDistance(pokemon.getPos(), playerPosition);

	pokemon.update(0.05, playerPosition);
	expectTrue(pokemon.getBehaviorState() == PokemonBehaviorState::Flee,
	           "nearby player switches a ground Pokemon into Flee state");
	const float firstFrameSpeed = glm::length(pokemon.getVelocity());
	expectTrue(firstFrameSpeed > 0.0f && firstFrameSpeed < 4.2f,
	           "flee movement accelerates instead of jumping to full speed");

	for (int i = 0; i < 40; ++i)
	{
		pokemon.update(0.05, playerPosition);
	}
	expectTrue(horizontalDistance(pokemon.getPos(), playerPosition) > startingDistance + 2.0f,
	           "flee steering increases distance from the player");
	expectTrue(glm::length(pokemon.getVelocity()) <= 4.2001f,
	           "flee speed remains under its configured cap");
}

void testFleeStateReturnsToWanderAfterReachingSafety()
{
	Pokemon pokemon(0, 3, 123u);
	pokemon.setPosition(glm::vec3(2.0f, 0.0f, 0.0f));
	pokemon.update(0.05, glm::vec3(0.0f));
	expectTrue(pokemon.getBehaviorState() == PokemonBehaviorState::Flee,
	           "test setup enters Flee state");

	const glm::vec3 distantPlayer(40.0f, 0.0f, 40.0f);
	for (int i = 0; i < 80; ++i)
	{
		pokemon.update(0.05, distantPlayer);
	}
	expectTrue(pokemon.getBehaviorState() != PokemonBehaviorState::Flee,
	           "Pokemon leaves Flee state after the threat is gone");
}

void testLargeFrameIsClampedAndCaughtPokemonStops()
{
	Pokemon regular(0, 5, 321u);
	Pokemon stalled(0, 5, 321u);
	const glm::vec3 distantPlayer(40.0f, 0.0f, 40.0f);
	regular.update(0.05, distantPlayer);
	stalled.update(3.0, distantPlayer);
	expectNear(glm::distance(regular.getPos(), stalled.getPos()), 0.0f, 0.0001f,
	           "long frame uses the same clamped AI step as a normal frame");

	regular.setCaught(1);
	const glm::vec3 caughtPosition = regular.getPos();
	for (int i = 0; i < 20; ++i)
	{
		regular.update(0.05, glm::vec3(0.0f));
	}
	expectNear(glm::distance(regular.getPos(), caughtPosition), 0.0f, 0.0001f,
	           "caught Pokemon stop updating immediately");
	expectNear(glm::length(regular.getVelocity()), 0.0f, 0.0001f,
	           "caught Pokemon clear residual velocity");
}

void testFlyingPokemonStaysWithinFlightBand()
{
	Pokemon flying(1, 4, 777u);
	for (int i = 0; i < 2000; ++i)
	{
		flying.update(0.05, glm::vec3(40.0f, 0.0f, 40.0f));
	}
	expectTrue(flying.getPos().y >= 12.0f && flying.getPos().y <= 30.0f,
	           "flying Pokemon remain inside the configured altitude band");
	expectTrue(std::fabs(flying.getPos().x) <= 46.0f && std::fabs(flying.getPos().z) <= 46.0f,
	           "flying Pokemon remain inside the playable field");
}
}

int main()
{
	testDeterministicSpawnAvoidsPlayerAndFieldEdge();
	testNearbyPlayerTriggersSmoothFleeMotion();
	testFleeStateReturnsToWanderAfterReachingSafety();
	testLargeFrameIsClampedAndCaughtPokemonStops();
	testFlyingPokemonStaysWithinFlightBand();

	if (failures != 0)
	{
		std::cerr << failures << " Pokemon behavior test(s) failed" << std::endl;
		return 1;
	}

	std::cout << "All Pokemon behavior tests passed" << std::endl;
	return 0;
}
