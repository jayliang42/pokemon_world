#include "ResearchRunState.h"

#include <algorithm>
#include <limits>

ResearchRunOutcome evaluateResearchRunOutcome(
	int caughtCount, int captureGoal, int pokeballs, int playerHealth,
	bool researchSubmitted)
{
	const int safeCaptureGoal = std::max(1, captureGoal);
	if (researchSubmitted)
	{
		return ResearchRunOutcome::ResearchSubmitted;
	}
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

CampSettlementSummary makeCampSettlement(
	const CampSettlementInput &input)
{
	CampSettlementSummary settlement;
	settlement.restoredHealth = std::max(1, input.playerMaximumHealth);
	settlement.replenishedPokeballs = std::max(1, input.startingPokeballs);
	const int captureGoal = std::max(1, input.captureGoal);
	settlement.eligible = input.atCamp && !input.alreadySubmitted &&
	                      input.caughtCount >= captureGoal;
	if (!settlement.eligible)
	{
		return settlement;
	}

	const long long captures = std::max(0, input.caughtCount);
	const long long defeats = std::max(0, input.defeatedCount);
	const long long objectives =
		std::max(0, std::min(9, input.completedObjectives));
	const long long score = captures * 100LL + defeats * 60LL +
	                        objectives * 50LL;
	settlement.researchScore = static_cast<int>(std::min(
		score, static_cast<long long>(std::numeric_limits<int>::max())));
	return settlement;
}
