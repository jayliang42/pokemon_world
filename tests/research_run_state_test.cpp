#include "ResearchRunState.h"

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

void testRunOutcomePrioritizesCompletionThenFainting()
{
	expectTrue(evaluateResearchRunOutcome(2, 5, 4, 80) ==
	               ResearchRunOutcome::Active,
	           "an in-progress healthy run remains active");
	expectTrue(evaluateResearchRunOutcome(5, 5, 0, 0) ==
	               ResearchRunOutcome::ResearchComplete,
	           "meeting the capture goal completes the run even on the final exchange");
	expectTrue(evaluateResearchRunOutcome(2, 5, 4, 0) ==
	               ResearchRunOutcome::PlayerFainted,
	           "zero player health produces a fainted run state");
	expectTrue(evaluateResearchRunOutcome(2, 5, 0, 80) ==
	               ResearchRunOutcome::OutOfPokeBalls,
	           "a healthy player with no Poke Balls reaches the inventory end state");
	expectTrue(evaluateResearchRunOutcome(5, 5, 10, 118, true) ==
	               ResearchRunOutcome::ResearchSubmitted,
	           "a submitted report is distinct from research ready to return");
}

void testCampSettlementRequiresCompletedResearchAtThePhysicalCamp()
{
	CampSettlementInput input;
	input.atCamp = true;
	input.caughtCount = 4;
	input.captureGoal = 5;
	expectTrue(!makeCampSettlement(input).eligible,
	           "an incomplete primary objective cannot be submitted");

	input.caughtCount = 5;
	input.atCamp = false;
	expectTrue(!makeCampSettlement(input).eligible,
	           "completed research cannot be submitted away from camp");

	input.atCamp = true;
	input.alreadySubmitted = true;
	expectTrue(!makeCampSettlement(input).eligible,
	           "the same research run cannot be submitted twice");
}

void testEligibleSettlementScoresAndReplenishesTheNextRunSupplies()
{
	CampSettlementInput input;
	input.atCamp = true;
	input.caughtCount = 5;
	input.captureGoal = 5;
	input.defeatedCount = 2;
	input.completedObjectives = 9;
	input.playerMaximumHealth = 118;
	input.startingPokeballs = 10;
	const CampSettlementSummary settlement = makeCampSettlement(input);
	expectTrue(settlement.eligible && settlement.researchScore == 1070,
	           "settlement rewards all nine species, regional, and field objectives");
	expectTrue(settlement.restoredHealth == 118 &&
	               settlement.replenishedPokeballs == 10,
	           "camp settlement restores health and replenishes field supplies");

	input.completedObjectives = 999;
	expectTrue(makeCampSettlement(input).researchScore == 1070,
	           "invalid objective totals cannot create research score beyond the mission");
}

void testCampRecoveryIsScopedToRecoverableFaints()
{
	expectTrue(canRecoverAtCamp(ResearchRunOutcome::PlayerFainted, 1),
	           "a fainted player with remaining Poke Balls can return to camp");
	expectTrue(!canRecoverAtCamp(ResearchRunOutcome::PlayerFainted, 0),
	           "camp recovery cannot bypass an empty Poke Ball inventory");
	expectTrue(!canRecoverAtCamp(ResearchRunOutcome::ResearchComplete, 4) &&
	               !canRecoverAtCamp(ResearchRunOutcome::OutOfPokeBalls, 0),
	           "camp recovery does not override completed or inventory-ended runs");
}

void testCampRecoveryAlwaysRestoresPlayableHealth()
{
	expectTrue(campRecoveryHealth(118) == 118,
	           "camp recovery restores the configured maximum health");
	expectTrue(campRecoveryHealth(0) == 1 && campRecoveryHealth(-20) == 1,
	           "invalid health limits still recover to a safe playable value");
}
}

int main()
{
	testRunOutcomePrioritizesCompletionThenFainting();
	testCampRecoveryIsScopedToRecoverableFaints();
	testCampRecoveryAlwaysRestoresPlayableHealth();
	testCampSettlementRequiresCompletedResearchAtThePhysicalCamp();
	testEligibleSettlementScoresAndReplenishesTheNextRunSupplies();
	if (failures != 0)
	{
		return 1;
	}
	std::cout << "Research run state tests passed" << std::endl;
	return 0;
}
