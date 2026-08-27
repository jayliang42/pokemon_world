#include "AlphaNest.h"

#include <cmath>

bool validateAlphaNestProgress(const AlphaNestProgress &progress)
{
	return !(progress.active && progress.resolved);
}

AlphaNestInteractionStatus evaluateAlphaNestInteraction(
	const AlphaNestProgress &progress,
	const AlphaNestInteractionInput &input)
{
	if (!validateAlphaNestProgress(progress) ||
	    !std::isfinite(input.distance) || input.distance < 0.0f ||
	    !std::isfinite(input.interactionRadius) ||
	    input.interactionRadius <= 0.0f)
	{
		return AlphaNestInteractionStatus::InvalidInput;
	}
	if (progress.resolved)
	{
		return AlphaNestInteractionStatus::Resolved;
	}
	if (progress.active)
	{
		return AlphaNestInteractionStatus::AlreadyActive;
	}
	if (!input.surveyActive)
	{
		return AlphaNestInteractionStatus::SurveyUnavailable;
	}
	if (!input.prerequisitesMet)
	{
		return AlphaNestInteractionStatus::Locked;
	}
	if (input.interactionBusy)
	{
		return AlphaNestInteractionStatus::InteractionBusy;
	}
	if (input.distance > input.interactionRadius)
	{
		return AlphaNestInteractionStatus::TooFar;
	}
	if (!input.grounded)
	{
		return AlphaNestInteractionStatus::Airborne;
	}
	return AlphaNestInteractionStatus::Available;
}

bool activateAlphaNest(AlphaNestProgress &progress,
	                   const AlphaNestInteractionInput &input)
{
	if (evaluateAlphaNestInteraction(progress, input) !=
	    AlphaNestInteractionStatus::Available)
	{
		return false;
	}
	progress.active = true;
	return true;
}

bool resolveAlphaNest(AlphaNestProgress &progress)
{
	if (!validateAlphaNestProgress(progress) || !progress.active)
	{
		return false;
	}
	progress.active = false;
	progress.resolved = true;
	return true;
}
