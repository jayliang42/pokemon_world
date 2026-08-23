#include "CaptureMechanics.h"

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

CaptureAttempt standardAttempt(PokemonSpecies species)
{
	CaptureAttempt attempt;
	attempt.species = species;
	attempt.distance = 2.5f;
	attempt.maximumDistance = 5.0f;
	attempt.alignment = 1.0f;
	attempt.activity = CaptureActivity::Moving;
	return attempt;
}

void testSpeciesHaveDistinctCaptureDifficulty()
{
	const float bulbasaur =
		calculateCaptureProbability(standardAttempt(PokemonSpecies::Bulbasaur));
	const float umbreon =
		calculateCaptureProbability(standardAttempt(PokemonSpecies::Umbreon));
	const float charizard =
		calculateCaptureProbability(standardAttempt(PokemonSpecies::Charizard));
	expectTrue(bulbasaur > umbreon && umbreon > charizard,
	           "species difficulty orders Bulbasaur, Umbreon, then Charizard");
}

void testDistanceAimAndActivityMatter()
{
	CaptureAttempt favorable = standardAttempt(PokemonSpecies::Umbreon);
	favorable.distance = 0.8f;
	favorable.activity = CaptureActivity::Idle;

	CaptureAttempt difficult = favorable;
	difficult.distance = difficult.maximumDistance;
	difficult.alignment = 0.2f;
	difficult.activity = CaptureActivity::Fleeing;

	expectTrue(calculateCaptureProbability(favorable) >
	               calculateCaptureProbability(difficult),
	           "close calm centered targets are easier than distant fleeing targets");
}

void testLowHealthImprovesCaptureProbability()
{
	CaptureAttempt healthy = standardAttempt(PokemonSpecies::Umbreon);
	healthy.healthRatio = 1.0f;
	CaptureAttempt weakened = healthy;
	weakened.healthRatio = 0.12f;
	expectTrue(calculateCaptureProbability(weakened) >
	               calculateCaptureProbability(healthy),
	           "weakening a Pokemon increases its capture probability");
}

void testProbabilityClampsInvalidExtremes()
{
	CaptureAttempt attempt = standardAttempt(PokemonSpecies::Bulbasaur);
	attempt.distance = -100.0f;
	attempt.maximumDistance = 0.0f;
	attempt.alignment = 4.0f;
	const float probability = calculateCaptureProbability(attempt);
	expectTrue(probability >= 0.12f && probability <= 0.95f,
	           "capture probability remains inside the playable range");
}

void testResolutionMapsSuccessAndNearMissShakes()
{
	const CaptureAttempt attempt = standardAttempt(PokemonSpecies::Umbreon);
	const float probability = calculateCaptureProbability(attempt);
	const CaptureResult success = resolveCaptureAttempt(attempt, probability * 0.5f);
	const CaptureResult nearMiss = resolveCaptureAttempt(
		attempt, probability + (1.0f - probability) * 0.10f);
	const CaptureResult middleMiss = resolveCaptureAttempt(
		attempt, probability + (1.0f - probability) * 0.50f);
	const CaptureResult cleanBreak = resolveCaptureAttempt(
		attempt, probability + (1.0f - probability) * 0.90f);
	expectTrue(success.captured && success.shakes == 3,
	           "successful captures complete all three shakes");
	expectTrue(!nearMiss.captured && nearMiss.shakes == 2,
	           "a close miss breaks after two shakes");
	expectTrue(!middleMiss.captured && middleMiss.shakes == 1,
	           "a middle miss breaks after one shake");
	expectTrue(!cleanBreak.captured && cleanBreak.shakes == 0,
	           "a poor throw breaks immediately");
}

void testCaptureRandomIsDeterministicAndBounded()
{
	CaptureRandom first(42u);
	CaptureRandom second(42u);
	for (int i = 0; i < 12; ++i)
	{
		const float firstRoll = first.nextUnit();
		const float secondRoll = second.nextUnit();
		expectNear(firstRoll, secondRoll, 0.0f,
		           "equal capture seeds produce the same sequence");
		expectTrue(firstRoll >= 0.0f && firstRoll < 1.0f,
		           "capture random rolls stay in [0, 1)");
	}
}
}

int main()
{
	testSpeciesHaveDistinctCaptureDifficulty();
	testDistanceAimAndActivityMatter();
	testLowHealthImprovesCaptureProbability();
	testProbabilityClampsInvalidExtremes();
	testResolutionMapsSuccessAndNearMissShakes();
	testCaptureRandomIsDeterministicAndBounded();

	if (failures != 0)
	{
		std::cerr << failures << " capture mechanic test(s) failed" << std::endl;
		return 1;
	}

	std::cout << "All capture mechanic tests passed" << std::endl;
	return 0;
}
