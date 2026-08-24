#include "ResearchRunState.h"

#include <algorithm>

ResearchRunOutcome evaluateResearchRunOutcome(
	int caughtCount, int captureGoal, int pokeballs, int playerHealth)
{
	const int safeCaptureGoal = std::max(1, captureGoal);
	if (caughtCount >= safeCaptureGoal)
	{
		return ResearchRunOutcome::ResearchComplete;
	}
	if (playerHealth <= 0)
	{
		return ResearchRunOutcome::PlayerFainted;
	}
	if (pokeballs <= 0)
	{
		return ResearchRunOutcome::OutOfPokeBalls;
	}
	return ResearchRunOutcome::Active;
}

bool canRecoverAtCamp(ResearchRunOutcome outcome, int pokeballs)
{
	return outcome == ResearchRunOutcome::PlayerFainted && pokeballs > 0;
}

int campRecoveryHealth(int maximumHealth)
{
	return std::max(1, maximumHealth);
}
