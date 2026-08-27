#include "AlphaNest.h"

#include <iostream>
#include <limits>
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

AlphaNestInteractionInput validInput()
{
	AlphaNestInteractionInput input;
	input.distance = 1.5f;
	input.interactionRadius = 2.8f;
	input.prerequisitesMet = true;
	input.surveyActive = true;
	input.grounded = true;
	return input;
}

void testActivationRequiresCompletedRegionalResearch()
{
	AlphaNestProgress progress;
	AlphaNestInteractionInput input = validInput();
	input.prerequisitesMet = false;
	expectTrue(evaluateAlphaNestInteraction(progress, input) ==
	               AlphaNestInteractionStatus::Locked &&
	               !activateAlphaNest(progress, input) && !progress.active,
	           "the Alpha nest remains locked until both regional surveys are complete");

	input.prerequisitesMet = true;
	expectTrue(activateAlphaNest(progress, input) && progress.active &&
	               !progress.resolved,
	           "completed regional research unlocks one real Alpha encounter");
	expectTrue(evaluateAlphaNestInteraction(progress, input) ==
	               AlphaNestInteractionStatus::AlreadyActive &&
	               !activateAlphaNest(progress, input),
	           "an active Alpha encounter cannot be started twice");
}

void testActivationUsesWorldAndInteractionConditions()
{
	AlphaNestProgress progress;
	AlphaNestInteractionInput input = validInput();
	input.surveyActive = false;
	expectTrue(evaluateAlphaNestInteraction(progress, input) ==
	               AlphaNestInteractionStatus::SurveyUnavailable,
	           "a closed survey cannot start the terminal encounter");
	input = validInput();
	input.interactionBusy = true;
	expectTrue(evaluateAlphaNestInteraction(progress, input) ==
	               AlphaNestInteractionStatus::InteractionBusy,
	           "capture or battle playback blocks Alpha activation");
	input = validInput();
	input.distance = input.interactionRadius + 0.01f;
	expectTrue(evaluateAlphaNestInteraction(progress, input) ==
	               AlphaNestInteractionStatus::TooFar,
	           "the player must enter the physical nest marker");
	input = validInput();
	input.grounded = false;
	expectTrue(evaluateAlphaNestInteraction(progress, input) ==
	               AlphaNestInteractionStatus::Airborne,
	           "the player must land before disturbing the nest");
}

void testOnlyAnActiveEncounterCanResolve()
{
	AlphaNestProgress progress;
	expectTrue(!resolveAlphaNest(progress),
	           "an untouched Alpha nest cannot be credited as resolved");
	AlphaNestInteractionInput input = validInput();
	expectTrue(activateAlphaNest(progress, input) && resolveAlphaNest(progress) &&
	               !progress.active && progress.resolved,
	           "a started encounter can resolve exactly once");
	expectTrue(!resolveAlphaNest(progress) &&
	               evaluateAlphaNestInteraction(progress, input) ==
	                   AlphaNestInteractionStatus::Resolved,
	           "resolved Alpha progress is stable and cannot be duplicated");
}

void testInvalidStateAndCoordinatesFailClosed()
{
	AlphaNestProgress invalid;
	invalid.active = true;
	invalid.resolved = true;
	expectTrue(!validateAlphaNestProgress(invalid) &&
	               evaluateAlphaNestInteraction(invalid, validInput()) ==
	                   AlphaNestInteractionStatus::InvalidInput,
	           "contradictory Alpha state fails closed");

	AlphaNestInteractionInput input = validInput();
	input.distance = std::numeric_limits<float>::quiet_NaN();
	expectTrue(evaluateAlphaNestInteraction(AlphaNestProgress(), input) ==
	               AlphaNestInteractionStatus::InvalidInput,
	           "non-finite nest distance is rejected");
	input = validInput();
	input.interactionRadius = 0.0f;
	expectTrue(evaluateAlphaNestInteraction(AlphaNestProgress(), input) ==
	               AlphaNestInteractionStatus::InvalidInput,
	           "a missing nest interaction radius is rejected");
}
}

int main()
{
	testActivationRequiresCompletedRegionalResearch();
	testActivationUsesWorldAndInteractionConditions();
	testOnlyAnActiveEncounterCanResolve();
	testInvalidStateAndCoordinatesFailClosed();
	if (failures != 0)
	{
		std::cerr << failures << " Alpha nest test(s) failed" << std::endl;
		return 1;
	}
	std::cout << "All Alpha nest tests passed" << std::endl;
	return 0;
}
