#include "PokemonPerception.h"

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

PokemonPerceptionInput standardInput()
{
	PokemonPerceptionInput input;
	input.observerPosition = glm::vec3(0.0f);
	input.observerHeading = 0.0f;
	input.subjectPosition = glm::vec3(0.0f, 0.0f, 6.0f);
	input.deltaSeconds = 0.5f;
	return input;
}

void testFrontSubjectRaisesAlertnessThroughVision()
{
	const PokemonPerceptionSample sample = samplePokemonPerception(
		PokemonPerceptionConfig(), standardInput());
	expectTrue(sample.visible && !sample.heard,
	           "a quiet subject in front is detected by vision only");
	expectTrue(sample.alertness > 0.0f,
	           "seeing a subject raises continuous alertness");
}

void testQuietSubjectBehindIsNotSeen()
{
	PokemonPerceptionInput input = standardInput();
	input.subjectPosition.z = -6.0f;
	const PokemonPerceptionSample sample = samplePokemonPerception(
		PokemonPerceptionConfig(), input);
	expectTrue(!sample.visible && !sample.heard,
	           "a quiet subject behind the observer stays outside the view cone");
	expectTrue(sample.alertness == 0.0f,
	           "an unseen quiet subject does not create alertness");
}

void testNoiseCanBeHeardOutsideTheViewCone()
{
	PokemonPerceptionInput input = standardInput();
	input.subjectPosition.z = -3.0f;
	input.subjectNoise = 1.0f;
	const PokemonPerceptionSample sample = samplePokemonPerception(
		PokemonPerceptionConfig(), input);
	expectTrue(!sample.visible && sample.heard,
	           "nearby noise is heard even when it comes from behind");
	expectTrue(sample.alertness > 0.0f,
	           "hearing a subject raises alertness more gradually");
}

void testOcclusionStopsVisionButNotNearbyHearing()
{
	PokemonPerceptionInput input = standardInput();
	input.subjectPosition.z = 3.0f;
	input.subjectNoise = 1.0f;
	input.lineOfSightClear = false;
	const PokemonPerceptionSample sample = samplePokemonPerception(
		PokemonPerceptionConfig(), input);
	expectTrue(!sample.visible && sample.heard,
	           "cover blocks sight without muting a nearby noisy subject");
	expectTrue(sample.alertness > 0.0f,
	           "hearing still raises alertness through visual cover");
}

void testVerticalToleranceLimitsVision()
{
	PokemonPerceptionInput input = standardInput();
	input.subjectPosition.y = 12.0f;
	const PokemonPerceptionSample sample = samplePokemonPerception(
		PokemonPerceptionConfig(), input);
	expectTrue(!sample.visible,
	           "a subject above the configured vertical tolerance is not visible");
}

void testAlertnessDecaysWithoutStimulusAndStaysNormalized()
{
	PokemonPerceptionInput input = standardInput();
	input.subjectPosition = glm::vec3(40.0f, 0.0f, 40.0f);
	input.currentAlertness = 0.6f;
	input.deltaSeconds = 1.0f;
	const PokemonPerceptionSample decayed = samplePokemonPerception(
		PokemonPerceptionConfig(), input);
	expectTrue(decayed.alertness < input.currentAlertness,
	           "alertness decays after the stimulus leaves");

	input.currentAlertness = 4.0f;
	input.deltaSeconds = 0.0f;
	const PokemonPerceptionSample clamped = samplePokemonPerception(
		PokemonPerceptionConfig(), input);
	expectTrue(clamped.alertness >= 0.0f && clamped.alertness <= 1.0f,
	           "alertness remains normalized for invalid external values");
}
}

int main()
{
	testFrontSubjectRaisesAlertnessThroughVision();
	testQuietSubjectBehindIsNotSeen();
	testNoiseCanBeHeardOutsideTheViewCone();
	testOcclusionStopsVisionButNotNearbyHearing();
	testVerticalToleranceLimitsVision();
	testAlertnessDecaysWithoutStimulusAndStaysNormalized();
	if (failures != 0)
	{
		return 1;
	}
	std::cout << "Pokemon perception tests passed" << std::endl;
	return 0;
}
