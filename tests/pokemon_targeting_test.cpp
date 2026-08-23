#include "PokemonTargeting.h"

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

PokemonTargetCandidate candidate(int index, const glm::vec3 &position,
	                              bool flying = false, bool caught = false)
{
	PokemonTargetCandidate value;
	value.index = index;
	value.position = position;
	value.flying = flying;
	value.caught = caught;
	return value;
}

void testIgnoresCaughtBehindAndOutOfRangeCandidates()
{
	std::vector<PokemonTargetCandidate> candidates = {
		candidate(0, glm::vec3(0.0f, 0.0f, -5.0f), false, true),
		candidate(1, glm::vec3(0.0f, 0.0f, 5.0f)),
		candidate(2, glm::vec3(0.0f, 0.0f, -20.0f)),
		candidate(3, glm::vec3(1.0f, 0.0f, -8.0f)),
	};
	PokemonTargetSelection selected = selectPokemonTarget(glm::vec3(0.0f), 0.0f, candidates);
	expectTrue(selected.valid() && selected.index == 3,
	           "selector ignores caught, behind-camera, and out-of-range ground Pokemon");
}

void testCenteredTargetCanBeatCloserEdgeTarget()
{
	std::vector<PokemonTargetCandidate> candidates = {
		candidate(0, glm::vec3(6.5f, 0.0f, -4.0f)),
		candidate(1, glm::vec3(0.0f, 0.0f, -9.0f)),
	};
	PokemonTargetSelection selected = selectPokemonTarget(glm::vec3(0.0f), 0.0f, candidates);
	expectTrue(selected.valid() && selected.index == 1,
	           "centered target wins when a closer candidate sits near the view edge");
	expectTrue(selected.alignment > 0.99f, "selected centered target records forward alignment");
}

void testFlyingTargetUsesLongerThreeDimensionalRange()
{
	std::vector<PokemonTargetCandidate> candidates = {
		candidate(4, glm::vec3(0.0f, 12.0f, -12.0f), true),
	};
	PokemonTargetSelection selected = selectPokemonTarget(glm::vec3(0.0f), 0.0f, candidates);
	expectTrue(selected.valid() && selected.flying && selected.index == 4,
	           "flying Pokemon can be selected inside their longer 3D targeting range");
	expectTrue(selected.distance > 16.9f && selected.distance < 17.1f,
	           "flying target distance includes altitude");
}

void testYawRotatesTheTargetingCone()
{
	std::vector<PokemonTargetCandidate> candidates = {
		candidate(5, glm::vec3(-8.0f, 0.0f, 0.0f)),
	};
	PokemonTargetSelection selected = selectPokemonTarget(glm::vec3(0.0f),
	                                                       1.5707963f, candidates);
	expectTrue(selected.valid() && selected.index == 5,
	           "targeting cone follows the player's yaw");
}
}

int main()
{
	testIgnoresCaughtBehindAndOutOfRangeCandidates();
	testCenteredTargetCanBeatCloserEdgeTarget();
	testFlyingTargetUsesLongerThreeDimensionalRange();
	testYawRotatesTheTargetingCone();

	if (failures != 0)
	{
		std::cerr << failures << " Pokemon targeting test(s) failed" << std::endl;
		return 1;
	}

	std::cout << "All Pokemon targeting tests passed" << std::endl;
	return 0;
}
