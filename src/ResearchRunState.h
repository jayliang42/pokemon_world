#pragma once

constexpr int RESEARCH_CAPTURE_GOAL = 5;
constexpr int RESEARCH_STARTING_POKEBALLS = 10;

enum class ResearchRunOutcome
{
	Active,
	ResearchComplete,
	ResearchSubmitted,
	PlayerFainted,
	OutOfPokeBalls,
};

ResearchRunOutcome evaluateResearchRunOutcome(
	int caughtCount, int captureGoal, int pokeballs, int playerHealth,
	bool researchSubmitted = false);
bool canRecoverAtCamp(ResearchRunOutcome outcome, int pokeballs);
int campRecoveryHealth(int maximumHealth);

struct CampSettlementInput
{
	bool atCamp = false;
	bool alreadySubmitted = false;
	int caughtCount = 0;
	int captureGoal = 1;
	int defeatedCount = 0;
	int completedObjectives = 0;
	int playerMaximumHealth = 1;
	int startingPokeballs = 1;
};

struct CampSettlementSummary
{
	bool eligible = false;
	int researchScore = 0;
	int restoredHealth = 1;
	int replenishedPokeballs = 1;
};

CampSettlementSummary makeCampSettlement(
	const CampSettlementInput &input);
