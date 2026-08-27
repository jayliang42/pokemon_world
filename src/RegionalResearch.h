#pragma once

#include "WorldLayout.h"

enum class RegionalObservationStatus
{
	Available,
	InvalidInput,
	NotResearchSite,
	SurveyUnavailable,
	AlreadyRecorded,
	InteractionBusy,
	TooFar,
	Airborne,
	RequiresNight,
};

struct RegionalObservationInput
{
	WorldInterestPointKind kind = WorldInterestPointKind::Trailhead;
	float distance = 0.0f;
	float interactionRadius = 1.0f;
	float daylight = 1.0f;
	bool grounded = false;
	bool surveyActive = false;
	bool interactionBusy = false;
	bool alreadyRecorded = false;
};

constexpr float MOONSHADOW_TRACKS_MAX_DAYLIGHT = 0.38f;

RegionalObservationStatus evaluateRegionalObservation(
	const RegionalObservationInput &input);
