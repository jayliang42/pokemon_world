#include "RegionalResearch.h"

#include <cmath>

RegionalObservationStatus evaluateRegionalObservation(
	const RegionalObservationInput &input)
{
	if (!std::isfinite(input.distance) || input.distance < 0.0f ||
	    !std::isfinite(input.interactionRadius) ||
	    input.interactionRadius <= 0.0f || !std::isfinite(input.daylight) ||
	    input.daylight < 0.0f || input.daylight > 1.0f)
	{
		return RegionalObservationStatus::InvalidInput;
	}
	if (input.kind != WorldInterestPointKind::MoonshadowTracks &&
	    input.kind != WorldInterestPointKind::RedrockLookout)
	{
		return RegionalObservationStatus::NotResearchSite;
	}
	if (!input.surveyActive)
	{
		return RegionalObservationStatus::SurveyUnavailable;
	}
	if (input.alreadyRecorded)
	{
		return RegionalObservationStatus::AlreadyRecorded;
	}
	if (input.interactionBusy)
	{
		return RegionalObservationStatus::InteractionBusy;
	}
	if (input.distance > input.interactionRadius)
	{
		return RegionalObservationStatus::TooFar;
	}
	if (!input.grounded)
	{
		return RegionalObservationStatus::Airborne;
	}
	if (input.kind == WorldInterestPointKind::MoonshadowTracks &&
	    input.daylight > MOONSHADOW_TRACKS_MAX_DAYLIGHT)
	{
		return RegionalObservationStatus::RequiresNight;
	}
	return RegionalObservationStatus::Available;
}
