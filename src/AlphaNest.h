#pragma once

constexpr float ALPHA_CAPTURE_DIFFICULTY_MULTIPLIER = 0.58f;

enum class AlphaNestInteractionStatus
{
	Available,
	InvalidInput,
	Locked,
	SurveyUnavailable,
	InteractionBusy,
	TooFar,
	Airborne,
	AlreadyActive,
	Resolved,
};

struct AlphaNestProgress
{
	bool active = false;
	bool resolved = false;
};

struct AlphaNestInteractionInput
{
	float distance = 0.0f;
	float interactionRadius = 0.0f;
	bool prerequisitesMet = false;
	bool surveyActive = false;
	bool interactionBusy = false;
	bool grounded = false;
};

bool validateAlphaNestProgress(const AlphaNestProgress &progress);
AlphaNestInteractionStatus evaluateAlphaNestInteraction(
	const AlphaNestProgress &progress,
	const AlphaNestInteractionInput &input);
bool activateAlphaNest(AlphaNestProgress &progress,
	                   const AlphaNestInteractionInput &input);
bool resolveAlphaNest(AlphaNestProgress &progress);
