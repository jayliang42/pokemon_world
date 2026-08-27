#include "RegionalResearch.h"

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

RegionalObservationInput validInput(WorldInterestPointKind kind)
{
	RegionalObservationInput input;
	input.kind = kind;
	input.distance = 1.2f;
	input.interactionRadius = 2.4f;
	input.daylight = 0.2f;
	input.grounded = true;
	input.surveyActive = true;
	return input;
}

void testMoonshadowTracksRequireAQuietNightLanding()
{
	RegionalObservationInput input = validInput(
		WorldInterestPointKind::MoonshadowTracks);
	expectTrue(evaluateRegionalObservation(input) ==
	               RegionalObservationStatus::Available,
	           "Moonshadow tracks are recordable after landing at night");

	input.daylight = MOONSHADOW_TRACKS_MAX_DAYLIGHT + 0.001f;
	expectTrue(evaluateRegionalObservation(input) ==
	               RegionalObservationStatus::RequiresNight,
	           "Moonshadow tracks wait for the dusk and night ecology window");
	input.daylight = MOONSHADOW_TRACKS_MAX_DAYLIGHT;
	input.grounded = false;
	expectTrue(evaluateRegionalObservation(input) ==
	               RegionalObservationStatus::Airborne,
	           "Moonshadow tracks cannot be recorded while flying over them");
}

void testRedrockLookoutRequiresArrivalButNotAClockWindow()
{
	RegionalObservationInput input = validInput(
		WorldInterestPointKind::RedrockLookout);
	input.daylight = 1.0f;
	expectTrue(evaluateRegionalObservation(input) ==
	               RegionalObservationStatus::Available,
	           "Redrock lookout remains surveyable during the day");
	input.daylight = 0.0f;
	expectTrue(evaluateRegionalObservation(input) ==
	               RegionalObservationStatus::Available,
	           "Redrock lookout remains surveyable at night");
	input.distance = input.interactionRadius + 0.001f;
	expectTrue(evaluateRegionalObservation(input) ==
	               RegionalObservationStatus::TooFar,
	           "a player outside the marked lookout cannot record it remotely");
}

void testUnavailableAndRepeatedInteractionsFailClosed()
{
	RegionalObservationInput input = validInput(
		WorldInterestPointKind::MoonshadowTracks);
	input.surveyActive = false;
	expectTrue(evaluateRegionalObservation(input) ==
	               RegionalObservationStatus::SurveyUnavailable,
	           "finished or failed runs cannot earn regional research");
	input.surveyActive = true;
	input.alreadyRecorded = true;
	expectTrue(evaluateRegionalObservation(input) ==
	               RegionalObservationStatus::AlreadyRecorded,
	           "the same regional observation is only credited once");
	input.alreadyRecorded = false;
	input.interactionBusy = true;
	expectTrue(evaluateRegionalObservation(input) ==
	               RegionalObservationStatus::InteractionBusy,
	           "battle and capture interactions take priority over field research");
	input.interactionBusy = false;
	input.kind = WorldInterestPointKind::Trailhead;
	expectTrue(evaluateRegionalObservation(input) ==
	               RegionalObservationStatus::NotResearchSite,
	           "the route fork guides travel but does not award research");
	input.kind = WorldInterestPointKind::AlphaNest;
	expectTrue(evaluateRegionalObservation(input) ==
	               RegionalObservationStatus::NotResearchSite,
	           "the Alpha nest cannot be credited as an ordinary regional survey");
}

void testInvalidSpatialAndLightingInputsAreRejected()
{
	RegionalObservationInput input = validInput(
		WorldInterestPointKind::RedrockLookout);
	input.distance = std::numeric_limits<float>::quiet_NaN();
	expectTrue(evaluateRegionalObservation(input) ==
	               RegionalObservationStatus::InvalidInput,
	           "non-finite distance cannot unlock a research site");
	input = validInput(WorldInterestPointKind::RedrockLookout);
	input.interactionRadius = 0.0f;
	expectTrue(evaluateRegionalObservation(input) ==
	               RegionalObservationStatus::InvalidInput,
	           "an invalid interaction radius fails closed");
	input = validInput(WorldInterestPointKind::RedrockLookout);
	input.daylight = 1.01f;
	expectTrue(evaluateRegionalObservation(input) ==
	               RegionalObservationStatus::InvalidInput,
	           "daylight outside the shared normalized range fails closed");
}
}

int main()
{
	testMoonshadowTracksRequireAQuietNightLanding();
	testRedrockLookoutRequiresArrivalButNotAClockWindow();
	testUnavailableAndRepeatedInteractionsFailClosed();
	testInvalidSpatialAndLightingInputsAreRejected();
	if (failures != 0)
	{
		return 1;
	}
	std::cout << "Regional research tests passed" << std::endl;
	return 0;
}
